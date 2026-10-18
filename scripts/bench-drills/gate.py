"""Universal gate, drill criteria, and the evidence block for the bench drills.

Everything here is pure: counters and measurements in, checks and text out.
The drill protocol is the last comment on issue #593.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable


@dataclass(frozen=True)
class EndCounters:
    """Counters read from one end of the link at one instant.

    None means the value could not be read (channel never emitted or went
    stale, event not in the dictionary or past its throttle, openocd not
    answering). The gate treats None as a failure; `why` holds the reason.
    traffic: (BytesSent, BytesReceived) on a probe-less board with no
    BootCount. A counter that goes down means the board rebooted.
    """

    config_failed: int | None
    rx_overrun: int | None
    rx_dropped: int | None
    halted: int | None
    boot_count: int | None = None
    has_probe: bool = True
    why: dict[str, str] = field(default_factory=dict)
    traffic: tuple[int | None, int | None] | None = None


@dataclass(frozen=True)
class Snapshot:
    """Counters for every end a drill uses, keyed by end name."""

    ends: dict[str, EndCounters]


@dataclass
class CommandTally:
    """Every logical command a drill sent, and whether it was confirmed."""

    sent: list[tuple[str, str, bool]] = field(default_factory=list)
    missing: list[tuple[str, str]] = field(default_factory=list)

    def record(self, end: str, command: str, confirmed: bool) -> None:
        """Record one logical command (all its re-sends count once)."""
        self.sent.append((end, command, confirmed))

    def record_missing(self, end: str, command: str) -> None:
        """Record a command the dictionary under test does not have."""
        self.missing.append((end, command))


@dataclass(frozen=True)
class Check:
    """One row of the pass/fail table."""

    name: str
    passed: bool
    detail: str
    gated: bool = True
    na: bool = False


# Counters the gate requires to stay flat, per end. The legacy GRC (D6) runs
# the SX1276 stack, which has no RxDropped channel or RxOverrun event.
GATED_COUNTERS: dict[str, tuple[str, ...]] = {
    "flight": ("boot_count", "config_failed", "rx_dropped", "rx_overrun", "halted"),
    "grc": ("config_failed", "rx_dropped", "rx_overrun", "halted"),
    "legacy": ("config_failed", "halted"),
}

COUNTER_LABELS = {
    "boot_count": "BootCount",
    "config_failed": "ConfigurationFailed",
    "rx_dropped": "RxDropped",
    "rx_overrun": "RxOverrun",
    "halted": "openocd halted",
}


def _delta_check(
    end: str, counter: str, b: EndCounters, a: EndCounters | None
) -> Check:
    name = f"{end} {COUNTER_LABELS[counter]} delta = 0"
    before = getattr(b, counter)
    after = getattr(a, counter) if a else None
    if before is None or after is None:
        reasons = [
            f"{when}: {s.why[counter]}"
            for when, s in (("before", b), ("after", a))
            if s and counter in s.why
        ]
        detail = f"unreadable (before={before}, after={after})"
        return Check(name, False, "; ".join([detail, *reasons]))
    delta = after - before
    return Check(name, delta == 0, f"{before} -> {after} (delta {delta})")


def _reboot_check(
    end: str,
    before: tuple[int | None, int | None],
    after: tuple[int | None, int | None] | None,
) -> Check:
    name = f"{end} BytesSent/BytesReceived never decrease (reboot detector)"
    if after is None or None in before or None in after:
        return Check(name, False, f"unreadable (before={before}, after={after})")
    return Check(
        name,
        all(x1 >= x0 for x0, x1 in zip(before, after)),
        f"{before} -> {after}",
    )


def universal_gate(
    before: Snapshot, after: Snapshot, tally: CommandTally
) -> list[Check]:
    """Apply the #593 universal gate. No exemptions: unreadable counts as FAIL."""
    checks = []
    for end, counters in GATED_COUNTERS.items():
        if end not in before.ends:
            continue
        b, a = before.ends[end], after.ends.get(end)
        for counter in counters:
            if counter == "halted" and not b.has_probe:
                name = f"{end} {COUNTER_LABELS[counter]} delta = 0"
                checks.append(Check(name, False, "no debug probe on this end", na=True))
                continue
            checks.append(_delta_check(end, counter, b, a))
        if b.traffic is not None:
            checks.append(_reboot_check(end, b.traffic, a.traffic if a else None))
    confirmed = sum(1 for _, _, ok in tally.sent if ok)
    total = len(tally.sent) + len(tally.missing)
    unconfirmed = [f"{e}:{c}" for e, c, ok in tally.sent if not ok]
    missing = [f"{e}:{c}" for e, c in tally.missing]
    detail = f"{confirmed}/{total} confirmed"
    if unconfirmed:
        detail += f"; unconfirmed {', '.join(unconfirmed)}"
    if missing:
        detail += f"; not in dictionary {', '.join(missing)}"
    checks.append(
        Check("commands N/N dispatched", total > 0 and confirmed == total, detail)
    )
    return checks


@dataclass(frozen=True)
class Criterion:
    """One drill-specific pass criterion, evaluated against a measurement.

    `test` gets the measured value and returns pass/fail. A criterion with
    gated=False is recorded in the table but does not decide the verdict.
    """

    name: str
    key: str
    expect: str
    test: Callable[[object], bool] = lambda v: True
    gated: bool = True


def evaluate(
    criteria: tuple[Criterion, ...], measurements: dict[str, object]
) -> list[Check]:
    """Evaluate a drill's criteria table. A missing measurement is a FAIL."""
    checks = []
    for c in criteria:
        if measurements.get(c.key) is None:
            checks.append(
                Check(c.name, not c.gated, f"not measured (expect {c.expect})", c.gated)
            )
            continue
        value = measurements[c.key]
        try:
            passed = bool(c.test(value))
        except (TypeError, ValueError, AttributeError):
            passed = False
        checks.append(Check(c.name, passed, f"{value} (expect {c.expect})", c.gated))
    return checks


def verdict(checks: list[Check]) -> bool:
    """A drill passes only if every gated, applicable check passes."""
    return all(c.passed for c in checks if c.gated and not c.na)


def render_evidence(
    drill_id: str,
    title: str,
    provenance: list[tuple[str, str]],
    before: Snapshot,
    after: Snapshot,
    checks: list[Check],
    notes: list[str],
    log_dir: str,
) -> str:
    """Render the evidence block posted as the per-drill results comment."""

    def boot(s: Snapshot) -> str:
        e = s.ends.get("flight")
        return "n/a" if e is None or e.boot_count is None else str(e.boot_count)

    ok = verdict(checks)
    lines = [
        f"### {drill_id}: {title} ({'PASS' if ok else 'FAIL'})",
        "",
        "| Field | Value |",
        "|---|---|",
    ]
    lines += [f"| {k} | {v} |" for k, v in provenance]
    lines += [
        f"| flight BootCount before / after | {boot(before)} / {boot(after)} |",
        f"| raw logs | `{log_dir}` |",
        "",
        "| Check | Result | Detail |",
        "|---|---|---|",
    ]
    for c in checks:
        if c.na:
            result = "N/A"
        elif c.gated:
            result = "PASS" if c.passed else "FAIL"
        else:
            result = "recorded"
        lines.append(f"| {c.name} | {result} | {c.detail} |")
    if notes:
        lines += ["", *[f"- {n}" for n in notes]]
    return "\n".join(lines) + "\n"
