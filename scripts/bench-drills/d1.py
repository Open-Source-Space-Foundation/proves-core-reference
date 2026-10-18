#!/usr/bin/env python3
"""D1 profile matrix and its sub-drills D1a, D1b, D1c, D1d, D1s (#593)."""

from __future__ import annotations

import sys

from gate import Criterion
from ops import (
    GUARD_S,
    PEER,
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


def run_d1b(rig: Rig) -> dict:
    """revert_s bounds: 0 and 901 rejected; 900 accepted and reverts by 945 s."""
    quiesce(rig)
    out = {}
    for revert_s in (0, 901):
        sent = rig.command("flight", "SET_RX_PROFILE", "P1_LORA_SF10", revert_s)
        out[f"revert_s={revert_s}"] = rig.outcome(sent)
    reverted0 = rig.count_events("flight", "ProfileReverted") or 0
    sent = rig.command("flight", "SET_RX_PROFILE", "P1_LORA_SF10", GUARD_S)
    out[f"revert_s={GUARD_S}"] = rig.outcome(sent)
    start = rig.now()
    if rig.wait_for(
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
    reverted0 = rig.count_events("flight", "ProfileReverted") or 0
    applied = set_profile(rig, "flight", "RX", "P1_LORA_SF10", revert_s=None)
    rig.sleep(minutes * 60)
    rig.settle(("flight",))
    reverts = (rig.count_events("flight", "ProfileReverted") or 0) - reverted0
    restore_boot_profiles(rig)
    return {"applied": applied, "minutes_observed": minutes, "reverts": reverts}


def run_d1d(rig: Rig) -> dict:
    """CONTINUOUS_WAVE(duration_ms): 1 and 10000 accepted, 0 and 10001 rejected."""
    quiesce(rig)
    out = {}
    for ms in (1, 10000, 0, 10001):
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
                "revert_s=0 rejected", "revert_s=0", "error", lambda v: v == "error"
            ),
            Criterion(
                "revert_s=901 rejected", "revert_s=901", "error", lambda v: v == "error"
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
        tuple(
            Criterion(
                f"cw {ms} ms {want}", f"cw {ms} ms", want, lambda v, w=want: v == w
            )
            for ms, want in (
                (1, "completed"),
                (10000, "completed"),
                (0, "error"),
                (10001, "error"),
            )
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
