#!/usr/bin/env python3
"""D1 profile matrix and its sub-drills D1a, D1b, D1c, D1d, D1s (#593)."""

from __future__ import annotations

import re
import sys

from gate import Criterion
from ops import (
    GUARD_S,
    PEER,
    delta,
    parity_cell,
    profile_events,
    quiesce,
    restore_boot_profiles,
    set_profile,
    transmit,
)
from rig import PROFILES, Drill, Rig, main

BOTH = ("flight", "grc")


def cell(profile: str, src: str) -> str:
    """Measurement key prefix of one matrix cell."""
    return f"{profile} {src}->{PEER[src]}"


def matrix_criteria(profiles: tuple[str, ...]) -> tuple[Criterion, ...]:
    """Byte parity and ProfileConfirmed for every profile in both directions."""
    rows = []
    for p in profiles:
        for src in ("grc", "flight"):
            k = cell(p, src)
            rows.append(
                Criterion(
                    f"{k} byte parity",
                    f"{k} parity",
                    "receiver delta == data CDC bytes",
                    lambda v: v.ok,
                )
            )
            rows.append(
                Criterion(
                    f"{k} ProfileConfirmed",
                    f"{k} confirmed",
                    "True",
                    lambda v: v is True,
                )
            )
    return tuple(rows)


def run_matrix(rig: Rig, profiles: tuple[str, ...]) -> dict:
    """Every profile, both directions; each cell ends back on P0."""
    out = {}
    for p in profiles:
        for src in ("grc", "flight"):
            r = parity_cell(rig, src, p)
            out[f"{cell(p, src)} parity"] = r["parity"]
            out[f"{cell(p, src)} confirmed"] = r["confirmed"]
    return out


SMOKE_PROFILES = ("P0_LORA_SF8", "P3_GFSK_38K")


def run_d1a(rig: Rig) -> dict:
    """A single CRC-good frame on a pending (guarded) flight RX profile confirms it."""
    profile = "P1_LORA_SF10"
    quiesce(rig)
    set_profile(rig, "flight", "RX", profile)
    set_profile(rig, "grc", "TX", profile)
    base = profile_events(rig, "flight", "ProfileConfirmed", profile)
    transmit(rig, "grc", True)
    rig.inject(1, 64, 3)
    transmit(rig, "grc", False)
    rig.settle(("flight",))
    confirmed = profile_events(rig, "flight", "ProfileConfirmed", profile) > base
    restore_boot_profiles(rig)
    return {"confirmed": confirmed}


REJECTED = ("VALIDATION_ERROR", "rejected by policy")


def set_rx_verdict(rig: Rig, revert_s: int) -> str | None:
    """SET_RX_PROFILE P1 with this revert_s: 'completed', the OpCodeError
    response, or 'rejected by policy'.

    #590 may reject a bad revert_s either by OpCodeError or by completing
    without switching and emitting a warning, so a completed command whose
    RxProfile did not move and which drew a radio warning counts as rejected."""
    mark = rig.event_mark("flight")
    rig.settle(("flight",))
    before = rig.channel("flight", "RxProfile")
    sent = rig.command("flight", "SET_RX_PROFILE", "P1_LORA_SF10", revert_s)
    result = rig.outcome(sent)
    if result != "completed":
        return result
    rig.settle(("flight",))
    radio = rig.dicts["flight"].radio_instance()
    warned = any(
        f".{radio}." in e.name and "WARNING" in e.severity
        for e in rig.events_since("flight", mark)
    )
    if warned and before is not None and rig.channel("flight", "RxProfile") == before:
        return "rejected by policy"
    return result


def run_d1b(rig: Rig) -> dict:
    """revert_s bounds: 0 and 901 rejected; 900 accepted and reverts by 945 s."""
    quiesce(rig)
    out = {}
    for revert_s in (0, 901):
        out[f"revert_s={revert_s}"] = set_rx_verdict(rig, revert_s)
    reverted0 = rig.count_events("flight", "ProfileReverted") or 0
    sent = rig.command("flight", "SET_RX_PROFILE", "P1_LORA_SF10", GUARD_S)
    out[f"revert_s={GUARD_S}"] = rig.outcome(sent)
    start = rig.now()
    if out[f"revert_s={GUARD_S}"] == "completed" and rig.wait_for(
        lambda: (rig.count_events("flight", "ProfileReverted") or 0) > reverted0,
        GUARD_S + 45,
        every=15,
    ):
        out["reverted_after_s"] = round(rig.now() - start)
    restore_boot_profiles(rig)
    return out


def run_d1c(rig: Rig) -> dict:
    """SET_RX_PROFILE_UNGUARDED never reverts."""
    minutes = rig.param("minutes", 20)
    quiesce(rig)
    reverted0 = rig.count_events("flight", "ProfileReverted")
    applied = set_profile(rig, "flight", "RX", "P1_LORA_SF10", revert_s=None)
    rig.sleep(minutes * 60)
    rig.settle(("flight",))
    # None when ProfileReverted is not in the dictionary: no event, no evidence.
    reverts = delta(reverted0, rig.count_events("flight", "ProfileReverted"))
    restore_boot_profiles(rig)
    return {"applied": applied, "minutes_observed": minutes, "reverts": reverts}


CW_MS = re.compile(r"(?i)(^|_)ms$|millis")
# Out-of-range values first: if the bounds are broken, a short carrier is the
# worst case. Rejection counts only as VALIDATION_ERROR, not BUSY.
CW_CASES = (
    (0, "VALIDATION_ERROR"),
    (10001, "VALIDATION_ERROR"),
    (1, "completed"),
    (10000, "completed"),
)


def cw_argument(rig: Rig) -> str | None:
    """The CONTINUOUS_WAVE argument list, as 'name: type', from the dictionary."""
    d = rig.dicts["flight"]
    full = d.find(d.commands, "CONTINUOUS_WAVE")
    if full is None:
        return None
    return ", ".join(f"{p['name']}: {p['type']['name']}" for p in d.params[full])


def run_d1d(rig: Rig) -> dict:
    """CONTINUOUS_WAVE(duration_ms): 1 and 10000 accepted, 0 and 10001 rejected.

    Refuses to key the carrier unless the dictionary says the one argument is
    milliseconds: legacy builds take seconds, and 10000 s blocks the radio."""
    arg = cw_argument(rig)
    out: dict = {"cw argument": arg}
    if arg is None or "," in arg or not CW_MS.search(arg.split(":")[0]):
        rig.log(
            f"REFUSED: CONTINUOUS_WAVE takes ({arg}), not one milliseconds argument"
        )
        return out
    quiesce(rig)
    for ms, _ in CW_CASES:
        sent = rig.command("flight", "CONTINUOUS_WAVE", ms)
        out[f"cw {ms} ms"] = rig.outcome(sent, wait=30)
    return out


DRILLS = [
    Drill(
        "D1",
        "Profile matrix",
        BOTH,
        lambda rig: run_matrix(rig, PROFILES),
        matrix_criteria(PROFILES),
    ),
    Drill(
        "D1a",
        "Stray-packet confirm",
        BOTH,
        run_d1a,
        (
            Criterion(
                "frame confirms pending RX profile",
                "confirmed",
                "True",
                lambda v: v is True,
            ),
        ),
    ),
    Drill(
        "D1b",
        "Revert cap",
        BOTH,
        run_d1b,
        (
            Criterion(
                "revert_s=900 accepted",
                f"revert_s={GUARD_S}",
                "completed",
                lambda v: v == "completed",
            ),
            Criterion(
                "reverts by 900 s + 45 s",
                "reverted_after_s",
                "<= 945 s",
                lambda v: v <= GUARD_S + 45,
            ),
            Criterion(
                "revert_s=0 rejected",
                "revert_s=0",
                " or ".join(REJECTED),
                lambda v: v in REJECTED,
            ),
            Criterion(
                "revert_s=901 rejected",
                "revert_s=901",
                " or ".join(REJECTED),
                lambda v: v in REJECTED,
            ),
        ),
    ),
    Drill(
        "D1c",
        "Unguarded RX profile",
        BOTH,
        run_d1c,
        (
            Criterion(
                "unguarded profile applied", "applied", "True", lambda v: v is True
            ),
            Criterion(
                "observed long enough",
                "minutes_observed",
                ">= 20 min",
                lambda v: v >= 20,
            ),
            Criterion("no revert", "reverts", "0", lambda v: v == 0),
        ),
    ),
    Drill(
        "D1d",
        "CW bounds",
        BOTH,
        run_d1d,
        (
            Criterion(
                "CW argument is milliseconds",
                "cw argument",
                "one *_ms argument",
                lambda v: "," not in v and bool(CW_MS.search(v.split(":")[0])),
            ),
        )
        + tuple(
            Criterion(
                f"cw {ms} ms {want}", f"cw {ms} ms", want, lambda v, w=want: v == w
            )
            for ms, want in CW_CASES
        ),
    ),
    Drill(
        "D1s",
        "Plain-config smoke",
        BOTH,
        lambda rig: run_matrix(rig, SMOKE_PROFILES),
        matrix_criteria(SMOKE_PROFILES),
        build="plain",
    ),
]

if __name__ == "__main__":
    sys.exit(main(DRILLS))
