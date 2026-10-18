"""Radio steps shared by the drills, ported from July's resume-0710 scripts.

parity_cell   parity-lenfix-r2.sh (episode, quiesce, receiver delta vs capture)
saturation    phase6-slice14c-r5.sh (255 B every 40 ms, 15 s warmup, 120 s window)
cpu_share     D4: uspRadio execution cycles over the RTT shell
"""

from __future__ import annotations

import re
from dataclasses import dataclass

from rig import BOOT_PROFILE, ENDS, Rig

GUARD_S = 900  # longest guarded RX window (#590)
# Parity bursts: about three frame airtimes apart, so nothing queues.
PARITY_PACE = {
    "P0_LORA_SF8": 1.0,
    "P1_LORA_SF10": 3.5,
    "P2_LORA_SF7": 0.6,
    "P3_GFSK_38K": 0.2,
}
# Saturation paces: July's slice17 sweep (P2 SF7 scaled from P0 SF8).
SATURATION_PACE = {
    "P0_LORA_SF8": 0.35,
    "P1_LORA_SF10": 1.2,
    "P2_LORA_SF7": 0.2,
    "P3_GFSK_38K": 0.04,
}
PEER = {"flight": "grc", "grc": "flight"}


def profile_events(rig: Rig, end: str, event: str, text: str) -> int:
    """Count ProfileChanged/Confirmed/Reverted events whose text mentions `text`."""
    return rig.count_events(end, event, text) or 0


def set_profile(
    rig: Rig, end: str, direction: str, profile: str, revert_s: int | None = GUARD_S
) -> bool:
    """Switch one direction of one end. revert_s=None sends SET_RX_PROFILE_UNGUARDED.

    The GRC is confirmed by its TxProfile/RxProfile channel; the flight by
    ProfileChanged or OpCodeCompleted (#591 acks after the work is done)."""
    if direction == "TX":
        cmd, args = "SET_TX_PROFILE", (profile,)
    elif revert_s is None:
        cmd, args = "SET_RX_PROFILE_UNGUARDED", (profile,)
    else:
        cmd, args = "SET_RX_PROFILE", (profile, revert_s)
    if ENDS[end].confirm_by == "channel":
        effect = lambda: rig.channel(end, f"{direction.title()}Profile") == profile  # noqa: E731
    else:
        text = f"{direction} profile changed to {profile}"
        base = profile_events(rig, end, "ProfileChanged", text)
        effect = lambda: profile_events(rig, end, "ProfileChanged", text) > base  # noqa: E731
    return rig.command(end, cmd, *args, effect=effect).confirmed


def transmit(rig: Rig, end: str, enabled: bool) -> bool:
    """TRANSMIT ENABLED/DISABLED. Re-sending is idempotent."""
    return rig.command(
        end, "TRANSMIT", "ENABLED" if enabled else "DISABLED", timeout=180
    ).confirmed


def quiesce(rig: Rig) -> None:
    """TX disabled on every radio end of this drill."""
    for end in ("flight", "grc"):
        if end in rig.ends:
            transmit(rig, end, False)


def restore_boot_profiles(rig: Rig) -> None:
    """Both directions of every radio end back to the Boot-Default Profile."""
    for end in ("flight", "grc"):
        if end in rig.ends:
            set_profile(rig, end, "TX", BOOT_PROFILE)
            set_profile(rig, end, "RX", BOOT_PROFILE, revert_s=None)


@dataclass(frozen=True)
class Parity:
    """Receiver BytesReceived delta against the bytes on the GRC data CDC."""

    counted: int | None
    truth: int

    @property
    def ok(self) -> bool:
        """Exact parity on a non-empty episode."""
        return self.truth > 0 and self.counted == self.truth

    def __str__(self) -> str:
        return f"receiver {self.counted} B vs data CDC {self.truth} B"


def delta(a: int | None, b: int | None) -> int | None:
    """b - a, or None if either is unreadable."""
    return None if a is None or b is None else b - a


def parity_cell(
    rig: Rig, src: str, profile: str, frames: int = 5, size: int = 200
) -> dict:
    """One D1 cell: TX off both ends, switch, enable, parity, back to P0.

    flight->grc: truth is the GRC data CDC capture (what the GRC received).
    grc->flight: truth is the bytes written into the GRC data CDC. GRC
    BytesSent lags, so it only serves as a quiesce detector."""
    dst = PEER[src]
    quiesce(rig)
    rx_ok = set_profile(rig, dst, "RX", profile)
    tx_ok = set_profile(rig, src, "TX", profile)
    switched = rx_ok and tx_ok
    confirmed0 = profile_events(rig, dst, "ProfileConfirmed", profile)
    if src == "flight":
        rx0 = rig.channel_int("grc", "BytesReceived")
        with rig.capture(f"{profile}-flight-to-grc") as cap:
            transmit(rig, "flight", True)
            rig.sleep(20)
            transmit(rig, "flight", False)
            rx1 = rig.wait_static(lambda: rig.channel_int("grc", "BytesReceived"))
        parity = Parity(delta(rx0, rx1), cap["bytes"])
    else:
        rig.settle(("flight",))
        rx0 = rig.channel_int("flight", "BytesReceived")
        transmit(rig, "grc", True)
        truth = rig.inject(frames, size, PARITY_PACE[profile])
        rig.wait_static(lambda: rig.channel_int("grc", "BytesSent"))
        transmit(rig, "grc", False)
        rig.settle(("flight",))
        parity = Parity(delta(rx0, rig.channel_int("flight", "BytesReceived")), truth)
    confirmed = profile_events(rig, dst, "ProfileConfirmed", profile) > confirmed0
    rig.log(
        f"cell {profile} {src}->{dst}: switched={switched} {parity} confirmed={confirmed}"
    )
    restore_boot_profiles(rig)
    return {"parity": parity, "confirmed": confirmed, "switched": switched}


def saturation(rig: Rig, profile: str, window: float = 120, warmup: float = 15) -> dict:
    """July's r5 method: GRC TX -> flight RX, paced flood, receiver-counter goodput."""
    quiesce(rig)
    set_profile(rig, "flight", "RX", profile)
    set_profile(rig, "grc", "TX", profile)
    with rig.flood(255, SATURATION_PACE[profile]) as flood:
        tx_base = rig.channel_int("grc", "BytesSent")
        rig.command(
            "grc",
            "TRANSMIT",
            "ENABLED",
            effect=lambda: rig.channel_int("grc", "BytesSent") not in (None, tx_base),
            timeout=180,
        )
        rig.sleep(warmup)
        rx0, d0, t0 = (
            rig.channel_int("flight", "BytesReceived"),
            rig.channel_int("flight", "RxDropped"),
            rig.now(),
        )
        rig.log(f"window start {profile}: flight rx={rx0} drops={d0}")
        while rig.now() - t0 < window:
            rig.sleep(15)
            rig.log(
                f"  rx={rig.channel_int('flight', 'BytesReceived')} grc tx={rig.channel_int('grc', 'BytesSent')}"
            )
        rx1, d1, dt = (
            rig.channel_int("flight", "BytesReceived"),
            rig.channel_int("flight", "RxDropped"),
            rig.now() - t0,
        )
    transmit(rig, "grc", False)
    restore_boot_profiles(rig)
    rx = delta(rx0, rx1)
    kbps = None if rx is None or dt <= 0 else round(rx * 8 / dt / 1000, 2)
    rig.log(
        f"{profile}: window {dt:.0f} s rx {rx} B goodput {kbps} kbps drops {delta(d0, d1)} flood {flood['written']} B"
    )
    return {"goodput_kbps": kbps, "window_s": round(dt), "rx_bytes": rx}


ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
THREAD = re.compile(r"^\s*\*?\s*(0x[0-9a-fA-F]+)\s+(\S.*?)\s*$")
CYCLES = re.compile(r"Total execution cycles:\s*(\d+)")
# The shell prints execution_cycles cast to uint32_t, so deltas are mod 2^32.
CYCLE_WRAP = 1 << 32


def thread_cycles(rig: Rig) -> dict[tuple[str, str], int]:
    """execution_cycles per (address, name) from `kernel thread list`."""
    for line in ("kernel thread list", "kernel threads"):
        text = ANSI.sub("", rig.shell("flight", line, wait=4))
        cycles, key = {}, None
        for row in text.splitlines():
            if m := THREAD.match(row):
                key = (m.group(1), m.group(2))
            elif (m := CYCLES.search(row)) and key:
                cycles[key] = int(m.group(1))
        if cycles:
            return cycles
    return {}


def cpu_share(rig: Rig, thread: str = "uspRadio", gap: float = 60) -> float | None:
    """Percent of all execution cycles spent in `thread` over `gap` seconds."""
    a = thread_cycles(rig)
    rig.sleep(gap)
    b = thread_cycles(rig)
    deltas = {k: (v - a.get(k, 0)) % CYCLE_WRAP for k, v in b.items()}
    mine = [d for (_, name), d in deltas.items() if thread in name]
    total = sum(deltas.values())
    if not mine or total == 0:
        rig.log(f"cpu_share: no {thread} thread or no cycles in the shell output")
        return None
    return round(100 * sum(mine) / total, 2)
