"""Bench I/O for the drills, and the runner every drill entry point calls.

A bench is described by one env file (benches/<name>.env). Each end of the
link (flight, grc, legacy, rf) is one F' GDS stack: a tty, a dictionary, a
log directory and a ZMQ endpoint pair. The drills read counters from the GDS
channel.log/event.log and send commands with fprime-cli, the method July's
scripts used (logs/resume-0710 in the NVMe archive).
"""

from __future__ import annotations

import argparse
import contextlib
import datetime
import errno
import fcntl
import glob
import hashlib
import json
import os
import platform
import re
import socket
import struct
import subprocess
import sys
import termios
import threading
import time
import tty
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterator

from gate import (
    Check,
    CommandTally,
    Criterion,
    EndCounters,
    Snapshot,
    evaluate,
    render_evidence,
    universal_gate,
    verdict,
)

DRILLS_DIR = Path(__file__).resolve().parent
OVERLAY = DRILLS_DIR / "observability.conf"
DEFAULT_BENCH = DRILLS_DIR / "benches" / "pi500-dev.env"
DEFAULT_LOG_ROOT = (
    "/Volumes/nvme-1tb-m4/proves/proves-gs-project/proves-core-reference/logs"
)
PLACEHOLDER = "TODO"
# OpenOCD's defaults belong to whatever server the bench already runs.
DEFAULT_OPENOCD_PORTS = {"3333", "4444", "6666"}
RTT_SHELL_CHANNEL = 1  # CONFIG_SHELL_BACKEND_RTT_BUFFER in observability.conf

PROFILES = ("P0_LORA_SF8", "P1_LORA_SF10", "P2_LORA_SF7", "P3_GFSK_38K")
BOOT_PROFILE = "P0_LORA_SF8"
# uspRadio: the USP Radio Path. lora: flight's legacy instance (as the #594
# fixture falls back). uhf: GRC main's legacy instance.
RADIO_INSTANCES = ("uspRadio", "lora", "uhf")


@dataclass(frozen=True)
class EndSpec:
    """What the drills may trust about one end (the #593 known traps).

    settle_s: wait before reading counters. Flight counters arrive only in the
      Beacon (~30 s cadence; the Radio TM packet never emits), so 45 s.
    confirm_by: how a command is confirmed. The GRC event stream lags its
      channels by tens of seconds under load, so GRC commands are confirmed by
      channel movement; flight commands by events.
    has_boot_count: the GRC has no BootCount; its reboots show as openocd
      `halted` plus ConfigurationFailed instead.
    is_board: the end is a board (has an ELF and gated counters). Whether it
      has a debug probe is a bench fact: <END>_PROBE_SERIAL empty or `none`.
    """

    name: str
    settle_s: int
    confirm_by: str
    has_boot_count: bool
    is_board: bool


ENDS = {
    "flight": EndSpec("flight", 45, "event", True, True),
    "grc": EndSpec("grc", 10, "channel", False, True),
    "legacy": EndSpec("legacy", 10, "event", False, True),
    # rf: a second GDS on the flight dictionary, talking through the legacy
    # GRC's data CDC (D6). It is a view of the flight, not a board.
    "rf": EndSpec("rf", 10, "event", False, False),
}

GDS_KEYS = ("TTY", "DICT", "REPO", "VENV", "GDS_LOGS", "GUI_PORT", "ZMQ_IN", "ZMQ_OUT")
NO_PROBE = ("", "none")
PROBE_KEYS = (
    "OPENOCD_GDB",
    "OPENOCD_TELNET",
    "OPENOCD_TCL",
    "OPENOCD_LOG",
)


class BenchError(Exception):
    """The bench file cannot support the requested drill. Fails before any I/O."""


class DrillAbort(Exception):
    """A drill step could not complete. The drill is recorded as FAIL."""


# --------------------------------------------------------------------------
# Bench file


def load_bench(path: Path) -> dict[str, str]:
    """Parse KEY=VALUE lines. ${VAR} expands from earlier keys, then the
    environment; an environment variable of the same name overrides a key."""
    values: dict[str, str] = {}
    for n, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise BenchError(f"{path}:{n}: expected KEY=VALUE, got {raw!r}")
        key, value = line.split("=", 1)
        value = value.strip().strip('"').strip("'")
        value = re.sub(
            r"\$\{(\w+)\}",
            lambda m: values.get(m.group(1), os.environ.get(m.group(1), "")),
            value,
        )
        values[key.strip()] = os.path.expanduser(value)
    for key in list(values):
        if key in os.environ:
            values[key] = os.environ[key]
    values["_PATH"] = str(path)
    return values


@dataclass(frozen=True)
class Bench:
    """A parsed bench file. Every device path and port comes from here."""

    values: dict[str, str]

    @property
    def path(self) -> str:
        """The bench file path."""
        return self.values["_PATH"]

    def get(self, end: str, key: str) -> str:
        """Return <END>_<KEY>, failing loudly when it is missing or a placeholder."""
        full = f"{end.upper()}_{key}"
        value = self.values.get(full, "")
        if not value or value.startswith(PLACEHOLDER):
            raise BenchError(
                f"{self.path}: {full} is unset or a {PLACEHOLDER} placeholder"
            )
        return value

    def has_probe(self, end: str) -> bool:
        """The end is a board and the bench names its debug probe."""
        serial = self.values.get(f"{end.upper()}_PROBE_SERIAL", "").lower()
        return ENDS[end].is_board and serial not in NO_PROBE

    def problems(self, ends: tuple[str, ...], extra: tuple[str, ...] = ()) -> list[str]:
        """List every missing or placeholder key the given ends need."""
        out = []
        for end in ends:
            keys = GDS_KEYS + (("ELF",) if ENDS[end].is_board else ())
            if self.has_probe(end):
                keys += ("PROBE_SERIAL",) + PROBE_KEYS
            for key in keys:
                try:
                    value = self.get(end, key)
                except BenchError as e:
                    out.append(str(e))
                    continue
                if (
                    key.startswith("OPENOCD_")
                    and key != "OPENOCD_LOG"
                    and value in DEFAULT_OPENOCD_PORTS
                ):
                    out.append(
                        f"{self.path}: {end.upper()}_{key}={value} is an openocd default port"
                    )
        for full in extra:
            end, key = full.split("_", 1)
            try:
                self.get(end.lower(), key)
            except BenchError as e:
                out.append(str(e))
        return out

    def gds_command(self, end: str) -> list[str]:
        """The command that starts this end's GDS with the logs the drills read."""
        g = lambda k: self.get(end, k)  # noqa: E731
        return [
            f"{g('VENV')}/bin/fprime-gds",
            "--gui=html",
            "--skip-browser-open",
            "--gui-port",
            g("GUI_PORT"),
            "--uart-device",
            g("TTY"),
            "--uart-baud",
            self.values.get(f"{end.upper()}_BAUD", "115200"),
            "--dictionary",
            g("DICT"),
            "--logs",
            g("GDS_LOGS"),
            "--log-directly",
            "--zmq-transport",
            g("ZMQ_IN"),
            g("ZMQ_OUT"),
        ]

    def openocd_command(self, end: str) -> list[str]:
        """The openocd command for this end: always its own probe serial and ports."""
        g = lambda k: self.get(end, k)  # noqa: E731
        return [
            self.values.get("OPENOCD", "openocd"),
            "-s",
            self.values.get("OPENOCD_SCRIPTS", "/usr/share/openocd/scripts"),
            "-f",
            "interface/cmsis-dap.cfg",
            "-c",
            f"adapter serial {g('PROBE_SERIAL')}",
            "-f",
            "target/rp2350.cfg",
            "-c",
            f"gdb port {g('OPENOCD_GDB')}",
            "-c",
            f"telnet port {g('OPENOCD_TELNET')}",
            "-c",
            f"tcl port {g('OPENOCD_TCL')}",
            "-l",
            g("OPENOCD_LOG"),
        ]


# --------------------------------------------------------------------------
# Dictionary and ELF


class Dictionary:
    """The F' JSON dictionary of the build under test on one end."""

    def __init__(self, data: dict):
        self.metadata = data.get("metadata", {})
        self.commands = {c["name"]: c["opcode"] for c in data.get("commands", [])}
        self.channels = {c["name"] for c in data.get("telemetryChannels", [])}
        self.events = {e["name"] for e in data.get("events", [])}
        self.constants = {
            c.get("qualifiedName", c.get("name")): c.get("value")
            for c in data.get("constants", [])
        }

    @classmethod
    def load(cls, path: str) -> "Dictionary":
        """Load a dictionary JSON file."""
        return cls(json.loads(Path(path).read_text()))

    def find(self, names: set[str] | dict, short: str) -> str | None:
        """Resolve a short name. Radio names try each RADIO_INSTANCES entry in
        order; anything else matches by suffix."""
        suffixes = [f".{i}.{short}" for i in RADIO_INSTANCES] + [f".{short}"]
        for suffix in suffixes:
            hits = sorted(n for n in names if n.endswith(suffix))
            if hits:
                return hits[0]
        return None

    def library_sha(self, lib: str) -> str:
        """The commit a library was built from, as recorded in the dictionary."""
        for entry in self.metadata.get("libraryVersions", []):
            if entry.startswith(f"{lib}@"):
                return entry.split("@", 1)[1]
        return "unknown"


def sha256(path: str) -> str:
    """sha256 of a file, or 'missing'."""
    try:
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()
    except OSError:
        return "missing"


def elf_symbol(path: str, name: str) -> int | None:
    """Address of a symbol in a 32-bit little-endian ELF (enough for RP2350)."""
    data = Path(path).read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 1:
        return None
    shoff, shentsize, shnum = (
        struct.unpack_from("<I", data, 0x20)[0],
        *struct.unpack_from("<HH", data, 0x2E),
    )
    sections = [
        struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize)
        for i in range(shnum)
    ]
    for sh in sections:
        if sh[1] != 2:  # SHT_SYMTAB
            continue
        strtab = sections[sh[6]]
        for off in range(sh[4], sh[4] + sh[5], 16):
            st_name, st_value = struct.unpack_from("<II", data, off)
            end = data.index(b"\0", strtab[4] + st_name)
            if data[strtab[4] + st_name : end].decode(errors="replace") == name:
                return st_value
    return None


def elf_has_overlay(path: str) -> bool | None:
    """True when the ELF was built with observability.conf (it links SEGGER RTT)."""
    try:
        return elf_symbol(path, "_SEGGER_RTT") is not None
    except OSError:
        return None


def git(repo: str, *args: str) -> str:
    """Run git in a repo and return stdout, or 'unknown'."""
    try:
        out = subprocess.run(
            ["git", "-C", repo, *args], capture_output=True, text=True, timeout=20
        )
    except (OSError, subprocess.TimeoutExpired):
        return "unknown"
    return out.stdout.strip() if out.returncode == 0 else "unknown"


def repo_sha(repo: str) -> str:
    """HEAD sha with a -dirty marker."""
    sha = git(repo, "rev-parse", "HEAD")
    dirty = git(repo, "status", "--porcelain", "--untracked-files=no")
    return f"{sha}{'-dirty' if dirty not in ('', 'unknown') else ''}"


def module_pins(repo: str) -> str:
    """Checked-out shas of the usp and usp_zephyr west modules, if present."""
    pins = []
    for name in ("usp", "usp_zephyr"):
        dirs = glob.glob(f"{repo}/lib/zephyr-workspace/modules/*/{name}") + glob.glob(
            f"{repo}/lib/zephyr-workspace/modules/{name}"
        )
        pins.append(
            f"{name}@{git(dirs[0], 'rev-parse', 'HEAD')[:12] if dirs else 'absent'}"
        )
    return " ".join(pins)


# --------------------------------------------------------------------------
# GDS logs


class GdsLog:
    """Incremental reader of one GDS log directory (channel.log, event.log).

    Line formats (fprime_gds DataLogger, verbose csv):
      channel: time,raw_time,name,id,value
      event:   time,raw_time,name,id,severity,text
    """

    def __init__(self, log_dir: str):
        self.dir = Path(log_dir)
        self.latest: dict[str, str] = {}
        self.updates: dict[str, int] = {}
        self.events: list[tuple[str, str]] = []
        self._offsets = {"channel.log": 0, "event.log": 0}
        self.start_offsets: dict[str, int] = {}
        self.poll()
        self.start_offsets = dict(self._offsets)

    def poll(self) -> None:
        """Read lines appended since the last poll."""
        for fname in self._offsets:
            path = self.dir / fname
            try:
                with path.open("rb") as f:
                    f.seek(self._offsets[fname])
                    chunk = f.read()
            except OSError:
                continue
            complete = chunk[: chunk.rfind(b"\n") + 1]
            self._offsets[fname] += len(complete)
            for line in complete.decode(errors="replace").splitlines():
                parts = line.split(",")
                if fname == "channel.log" and len(parts) >= 5:
                    self.latest[parts[2]] = ",".join(parts[4:])
                    self.updates[parts[2]] = self.updates.get(parts[2], 0) + 1
                elif fname == "event.log" and len(parts) >= 6:
                    self.events.append((parts[2], ",".join(parts[5:])))

    def archive(self, dest: Path, prefix: str) -> None:
        """Copy the lines appended during the drill into the run directory."""
        for fname, start in self.start_offsets.items():
            with contextlib.suppress(OSError):
                with (self.dir / fname).open("rb") as f:
                    f.seek(start)
                    (dest / f"{prefix}-{fname}").write_bytes(f.read())


# --------------------------------------------------------------------------
# Rig


@dataclass
class Sent:
    """One logical command: its opcode and the response counts before it."""

    end: str
    name: str
    opcode: int | None
    confirmed: bool
    completed_base: int = 0
    error_base: int = 0


@dataclass
class Rig:
    """Live bench I/O. DryRig overrides the primitives for offline runs."""

    bench: Bench
    ends: tuple[str, ...]
    run_dir: Path
    params: dict[str, str] = field(default_factory=dict)
    tally: CommandTally = field(default_factory=CommandTally)
    notes: list[str] = field(default_factory=list)

    def __post_init__(self) -> None:
        self.dicts = {e: self._load_dictionary(e) for e in self.ends}
        self.logs = {e: self._open_log(e) for e in self.ends}
        self._transcript = (self.run_dir / "transcript.log").open("a")

    # ---- primitives (overridden by DryRig)

    def _load_dictionary(self, end: str) -> Dictionary:
        return Dictionary.load(self.bench.get(end, "DICT"))

    def _open_log(self, end: str):
        return GdsLog(self.bench.get(end, "GDS_LOGS"))

    def now(self) -> float:
        """Seconds on the drill clock."""
        return time.time()

    def sleep(self, seconds: float) -> None:
        """Wait on the drill clock."""
        time.sleep(seconds)

    def _send_once(self, end: str, full: str, args: tuple[str, ...]) -> None:
        b = self.bench
        cmd = [
            f"{b.get(end, 'VENV')}/bin/fprime-cli",
            "command-send",
            full,
            "--arguments",
            *args,
            "--dictionary",
            b.get(end, "DICT"),
            "--zmq-transport",
            b.get(end, "ZMQ_IN"),
            b.get(end, "ZMQ_OUT"),
        ]
        out = subprocess.run(
            cmd, cwd=b.get(end, "REPO"), capture_output=True, text=True, timeout=60
        )
        if out.returncode != 0:
            self.log(
                f"  fprime-cli rc={out.returncode}: {(out.stderr or out.stdout).strip()[-300:]}"
            )

    def _open_tty(self, path: str, flags: int) -> int:
        fd = os.open(path, flags | os.O_NOCTTY | os.O_NONBLOCK)
        tty.setraw(fd)
        return fd

    def halted(self, end: str) -> int | None:
        """Lines mentioning `halted` in this end's openocd log."""
        try:
            text = Path(self.bench.get(end, "OPENOCD_LOG")).read_text(errors="replace")
        except (OSError, BenchError):
            return None
        return sum(1 for line in text.splitlines() if "halted" in line)

    def openocd(self, end: str, line: str, wait: float = 2.0) -> str:
        """Run one command on this end's openocd telnet port."""
        return self._tcp_exchange(
            int(self.bench.get(end, "OPENOCD_TELNET")), line + "\n", wait
        )

    def shell(self, end: str, line: str, wait: float = 3.0) -> str:
        """Run one Zephyr shell command over the RTT shell channel."""
        return self._tcp_exchange(
            int(self.bench.get(end, "RTT_SHELL_PORT")), line + "\r\n", wait
        )

    def start_rtt_shell(self) -> None:
        """Point the flight openocd at the RTT control block and serve the shell channel."""
        try:
            addr = elf_symbol(self.bench.get("flight", "ELF"), "_SEGGER_RTT")
        except OSError as e:
            raise DrillAbort(f"cannot read the flight ELF: {e}") from e
        if addr is None:
            raise DrillAbort(
                "flight ELF has no _SEGGER_RTT symbol: build it with observability.conf"
            )
        port = self.bench.get("flight", "RTT_SHELL_PORT")
        for line in (
            f'rtt setup {addr:#x} 1024 "SEGGER RTT"',
            "rtt polling_interval 10",
            "rtt start",
            f"rtt server start {port} {RTT_SHELL_CHANNEL}",
        ):
            self.log(
                f"  openocd flight: {line} -> {self.openocd('flight', line).strip()[-160:]!r}"
            )

    def _tcp_exchange(self, port: int, payload: str, wait: float) -> str:
        with socket.create_connection(("127.0.0.1", port), timeout=5) as s:
            s.settimeout(0.3)
            s.sendall(payload.encode())
            out, deadline = b"", time.time() + wait
            while time.time() < deadline:
                with contextlib.suppress(socket.timeout):
                    chunk = s.recv(65536)
                    if not chunk:
                        break
                    out += chunk
        return bytes(b for b in out if b < 0xF0).decode(errors="replace")

    def inject(self, frames: int, size: int, pace: float) -> int:
        """Write frames into the GRC data CDC (the GRC transmits them). Returns bytes written."""
        fd = self._open_tty(self.bench.get("grc", "DATA_TTY"), os.O_WRONLY)
        written = 0
        try:
            for _ in range(frames):
                with contextlib.suppress(BlockingIOError):
                    written += os.write(fd, bytes(i % 256 for i in range(size)))
                self.sleep(pace)
        finally:
            os.close(fd)
        return written

    @contextlib.contextmanager
    def flood(self, size: int, pace: float) -> Iterator[dict]:
        """Paced flood into the GRC data CDC (July's r5 method), then wait for
        the OS OUT queue to drain: a killed flood replays into the next boot."""
        path = self.bench.get("grc", "DATA_TTY")
        fd = self._open_tty(path, os.O_WRONLY)
        stats, stop = {"written": 0}, threading.Event()

        def pump() -> None:
            chunk = bytes(range(size))
            while not stop.is_set():
                with contextlib.suppress(BlockingIOError):
                    stats["written"] += os.write(fd, chunk)
                time.sleep(pace)

        t = threading.Thread(target=pump, daemon=True)
        t.start()
        try:
            yield stats
        finally:
            stop.set()
            t.join()
            deadline = time.time() + 150
            while time.time() < deadline:
                q = struct.unpack(
                    "i", fcntl.ioctl(fd, termios.TIOCOUTQ, struct.pack("i", 0))
                )[0]
                if q == 0:
                    break
                time.sleep(5)
            else:
                self.notes.append(f"{path} OUT queue never drained after the flood")
            os.close(fd)

    @contextlib.contextmanager
    def capture(self, label: str) -> Iterator[dict]:
        """Capture the GRC data CDC (what the GRC received over RF) to a file.

        The capture is the truth for byte parity: GRC BytesSent lags."""
        path = self.bench.get("grc", "DATA_TTY")
        out_path = self.run_dir / f"capture-{label}.bin"
        fd = self._open_tty(path, os.O_RDONLY)
        stats, stop = {"bytes": 0, "path": str(out_path)}, threading.Event()
        drain_until = time.time() + 2

        def reader() -> None:
            with out_path.open("wb") as out:
                while not stop.is_set():
                    try:
                        data = os.read(fd, 4096)
                    except OSError as e:
                        if e.errno != errno.EAGAIN:
                            raise
                        time.sleep(0.005)
                        continue
                    if time.time() >= drain_until:
                        out.write(data)
                        stats["bytes"] += len(data)

        t = threading.Thread(target=reader, daemon=True)
        t.start()
        time.sleep(2)
        try:
            yield stats
        finally:
            time.sleep(3)
            stop.set()
            t.join()
            os.close(fd)

    # ---- built on the primitives

    def log(self, message: str) -> None:
        """Print and record one transcript line."""
        line = f"[{datetime.datetime.now():%H:%M:%S}] {message}"
        print(line, flush=True)
        self._transcript.write(line + "\n")
        self._transcript.flush()

    def param(self, key: str, default: float) -> float:
        """A numeric drill parameter (--param key=value)."""
        return float(self.params.get(key, default))

    def channel(self, end: str, short: str) -> str | None:
        """Latest value of a channel, or None if it never emitted or is not in the dictionary."""
        self.logs[end].poll()
        full = self.dicts[end].find(self.dicts[end].channels, short)
        return self.logs[end].latest.get(full) if full else None

    def channel_int(self, end: str, short: str) -> int | None:
        """A channel as an int (None if unreadable)."""
        v = self.channel(end, short)
        try:
            return int(float(v)) if v is not None else None
        except ValueError:
            return None

    def channel_updates(self, end: str, short: str) -> int:
        """How many samples of a channel have arrived since the drill began."""
        self.logs[end].poll()
        full = self.dicts[end].find(self.dicts[end].channels, short)
        return self.logs[end].updates.get(full, 0) if full else 0

    def count_events(self, end: str, short: str, contains: str = "") -> int | None:
        """Events of this name whose text contains `contains`. None if not in the dictionary."""
        self.logs[end].poll()
        full = self.dicts[end].find(self.dicts[end].events, short)
        if full is None:
            return None
        return sum(
            1 for n, text in self.logs[end].events if n == full and contains in text
        )

    def opcode_events(self, end: str, event: str, opcode: int) -> int:
        """cmdDisp events (OpCodeDispatched/Completed/Error) naming this opcode."""
        self.logs[end].poll()
        full = self.dicts[end].find(self.dicts[end].events, event)
        hits = 0
        for n, text in self.logs[end].events:
            if n == full and any(
                int(h, 16) == opcode for h in re.findall(r"0x[0-9a-fA-F]+", text)
            ):
                hits += 1
        return hits

    def command(
        self,
        end: str,
        short: str,
        *args: object,
        effect: Callable[[], bool] | None = None,
        timeout: float = 120,
        every: float = 8,
    ) -> Sent:
        """Send a command, re-sending until confirmed (allowed by #593).

        Confirmation is `effect()` when given, else an OpCodeDispatched for the
        opcode. A command missing from the dictionary is a FAIL, not a crash."""
        d = self.dicts[end]
        full = d.find(d.commands, short)
        if full is None:
            self.tally.record_missing(end, short)
            self.log(f"FAIL {end}: {short} is not in the dictionary under test")
            return Sent(end, short, None, False)
        opcode = d.commands[full]
        base = self.opcode_events(end, "OpCodeDispatched", opcode)
        sent = Sent(
            end,
            full,
            opcode,
            False,
            self.opcode_events(end, "OpCodeCompleted", opcode),
            self.opcode_events(end, "OpCodeError", opcode),
        )
        confirm = effect or (
            lambda: self.opcode_events(end, "OpCodeDispatched", opcode) > base
        )
        argv = tuple(str(a) for a in args)
        start = self.now()
        self.log(f"{end} <- {short} {' '.join(argv)}")
        while self.now() - start < timeout:
            self._send_once(end, full, argv)
            deadline = self.now() + every
            while self.now() < deadline:
                if confirm():
                    sent.confirmed = True
                    self.tally.record(end, short, True)
                    return sent
                self.sleep(1)
        self.log(f"  {end} {short}: not confirmed after {timeout:.0f} s")
        self.tally.record(end, short, False)
        return sent

    def outcome(self, sent: Sent, wait: float = 15) -> str | None:
        """'completed' or 'error' for a sent command, waiting up to `wait` s."""
        if sent.opcode is None:
            return None
        deadline = self.now() + wait
        while True:
            if (
                self.opcode_events(sent.end, "OpCodeError", sent.opcode)
                > sent.error_base
            ):
                return "error"
            if (
                self.opcode_events(sent.end, "OpCodeCompleted", sent.opcode)
                > sent.completed_base
            ):
                return "completed"
            if self.now() >= deadline:
                return None
            self.sleep(1)

    def wait_for(
        self, predicate: Callable[[], bool], timeout: float, every: float = 5
    ) -> bool:
        """Poll a predicate on the drill clock."""
        deadline = self.now() + timeout
        while self.now() < deadline:
            if predicate():
                return True
            self.sleep(every)
        return predicate()

    def wait_static(
        self, read: Callable[[], object], every: float = 10, timeout: float = 300
    ) -> object:
        """Wait until a counter stops moving (quiesce), return its value."""
        prev, deadline = read(), self.now() + timeout
        while self.now() < deadline:
            self.sleep(every)
            cur = read()
            if cur == prev:
                return cur
            prev = cur
        return prev

    def settle(self, ends: tuple[str, ...] | None = None) -> None:
        """Wait long enough for each end's counters to be current."""
        self.sleep(max(ENDS[e].settle_s for e in (ends or self.ends)))

    def snapshot(self) -> Snapshot:
        """Counters for the universal gate (call after settle())."""
        out = {}
        for end in self.ends:
            if not ENDS[end].is_board:
                continue
            probe = self.bench.has_probe(end)
            out[end] = EndCounters(
                config_failed=self.count_events(end, "ConfigurationFailed"),
                rx_overrun=self.count_events(end, "RxOverrun"),
                rx_dropped=self.channel_int(end, "RxDropped"),
                halted=self.halted(end) if probe else None,
                has_probe=probe,
                boot_count=self.channel_int(end, "startupManager.BootCount")
                if ENDS[end].has_boot_count
                else None,
            )
        return Snapshot(out)

    def provenance(self) -> tuple[list[tuple[str, str]], list[Check]]:
        """Evidence fields, plus checks that both ends run what the bench says."""
        b, rows, checks = self.bench, [], []
        rows.append(
            (
                "bench",
                f"{b.values.get('BENCH_HOST_LABEL', '?')} on {socket.gethostname()} ({b.path})",
            )
        )
        topo = self.run_dir / "usb-topology.txt"
        topo.write_text(usb_topology())
        rows.append(("USB topology", f"`{topo}`"))
        scripts = str(DRILLS_DIR)
        dirty = git(scripts, "status", "--porcelain", "--", ".") not in ("", "unknown")
        rows.append(
            (
                "drill scripts",
                f"tree {git(scripts, 'rev-parse', 'HEAD:./')}{' + uncommitted edits' if dirty else ''}"
                f" at {git(scripts, 'rev-parse', 'HEAD')}",
            )
        )
        rows.append(("overlay sha", git(str(DRILLS_DIR), "hash-object", str(OVERLAY))))
        for end in self.ends:
            d = self.dicts[end]
            repo = b.values.get(f"{end.upper()}_REPO", "")
            rows.append(
                (
                    f"{end} repo sha",
                    f"{repo_sha(repo)} ({git(repo, 'rev-parse', '--abbrev-ref', 'HEAD')})",
                )
            )
            rows.append((f"{end} fprime-zephyr", d.library_sha("fprime-zephyr")))
            rows.append((f"{end} usp pins", module_pins(repo)))
            if ENDS[end].is_board:
                elf = b.values.get(f"{end.upper()}_ELF", "")
                elf_sha = sha256(elf)
                rows.append((f"{end} ELF sha256", elf_sha))
                expected = b.values.get(f"{end.upper()}_ELF_SHA256")
                if expected:
                    checks.append(
                        Check(
                            f"{end} ELF is the expected build",
                            elf_sha == expected,
                            f"expected {expected}",
                        )
                    )
                rows.append((f"{end} ELF links SEGGER RTT", str(elf_has_overlay(elf))))
                version = d.metadata.get("projectVersion", "")
                in_elf = bool(version) and version.encode() in _read(elf)
                checks.append(
                    Check(
                        f"{end} dictionary matches ELF",
                        in_elf,
                        f"projectVersion {version!r} {'found' if in_elf else 'NOT found'} in ELF",
                    )
                )
        return rows, checks


def _read(path: str) -> bytes:
    try:
        return Path(path).read_bytes()
    except OSError:
        return b""


def usb_topology() -> str:
    """USB tree of the bench host, for the evidence."""
    cmd = (
        ["lsusb", "-t"]
        if platform.system() == "Linux"
        else ["system_profiler", "SPUSBDataType"]
    )
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=60).stdout
    except (OSError, subprocess.TimeoutExpired) as e:
        return f"unavailable: {e}"


# --------------------------------------------------------------------------
# Dry run: the same drill code against a fake bench on a virtual clock.

DRY_DICTIONARY = {
    "metadata": {
        "projectVersion": "dry-run",
        "libraryVersions": ["fprime-zephyr@dryrun"],
    },
    "commands": [
        {"name": f"ReferenceDeployment.uspRadio.{c}", "opcode": 0x10017000 + i}
        for i, c in enumerate(
            (
                "TRANSMIT",
                "SET_TX_PROFILE",
                "SET_RX_PROFILE",
                "SET_RX_PROFILE_UNGUARDED",
                "CONTINUOUS_WAVE",
            )
        )
    ]
    + [{"name": "CdhCore.cmdDisp.CMD_NO_OP", "opcode": 0x01000000}],
    "telemetryChannels": [
        {"name": f"ReferenceDeployment.uspRadio.{c}"}
        for c in (
            "BytesSent",
            "BytesReceived",
            "TxProfile",
            "RxProfile",
            "RxDropped",
            "RxReverts",
        )
    ]
    + [{"name": "ReferenceDeployment.startupManager.BootCount"}],
    "events": [
        {"name": f"ReferenceDeployment.uspRadio.{e}"}
        for e in (
            "ProfileChanged",
            "ProfileConfirmed",
            "ProfileReverted",
            "ConfigurationFailed",
            "RxOverrun",
        )
    ]
    + [
        {"name": f"CdhCore.cmdDisp.{e}"}
        for e in ("OpCodeDispatched", "OpCodeCompleted", "OpCodeError")
    ],
    "constants": [{"qualifiedName": "USP_RADIO_RADIOHEAD_HEADER", "value": True}],
}


class FakeLog:
    """In-memory stand-in for GdsLog."""

    def __init__(self) -> None:
        self.latest = {
            "ReferenceDeployment.startupManager.BootCount": "57",
            "ReferenceDeployment.uspRadio.RxDropped": "0",
            "ReferenceDeployment.uspRadio.BytesReceived": "0",
            "ReferenceDeployment.uspRadio.BytesSent": "0",
        }
        self.updates: dict[str, int] = {}
        self.events: list[tuple[str, str]] = []

    def poll(self) -> None:
        """Nothing to read."""

    def archive(self, dest: Path, prefix: str) -> None:
        """Write the fake event stream so the run directory looks like a real one."""
        (dest / f"{prefix}-event.log").write_text(
            "\n".join(f"{n},{t}" for n, t in self.events) + "\n"
        )


class DryRig(Rig):
    """Runs drill logic offline: commands dispatch, counters stay flat, no bytes move."""

    def __post_init__(self) -> None:
        self.clock = 0.0
        super().__post_init__()

    def _load_dictionary(self, end: str) -> Dictionary:
        path = self.bench.values.get(f"{end.upper()}_DICT", "")
        if path and Path(path).is_file():
            return Dictionary.load(path)
        return Dictionary(DRY_DICTIONARY)

    def _open_log(self, end: str) -> FakeLog:
        return FakeLog()

    def now(self) -> float:
        """Virtual clock."""
        return self.clock

    def sleep(self, seconds: float) -> None:
        """Advance the virtual clock."""
        self.clock += seconds

    def _send_once(self, end: str, full: str, args: tuple[str, ...]) -> None:
        log, op = self.logs[end], self.dicts[end].commands[full]
        log.events.append(
            ("CdhCore.cmdDisp.OpCodeDispatched", f"Opcode {op:#x} dispatched to port 1")
        )
        short = full.rsplit(".", 1)[1]
        bad = (short == "SET_RX_PROFILE" and not 1 <= int(args[1]) <= 900) or (
            short == "CONTINUOUS_WAVE" and not 1 <= int(args[0]) <= 10000
        )
        if bad:
            log.events.append(
                (
                    "CdhCore.cmdDisp.OpCodeError",
                    f"Opcode {op:#x} completed with error VALIDATION_ERROR",
                )
            )
            return
        if short.startswith("SET_"):
            direction = "TX" if "TX" in short else "RX"
            log.latest[f"ReferenceDeployment.uspRadio.{direction.title()}Profile"] = (
                args[0]
            )
            log.events.append(
                (
                    "ReferenceDeployment.uspRadio.ProfileChanged",
                    f"{direction} profile changed to {args[0]}",
                )
            )
        log.events.append(
            ("CdhCore.cmdDisp.OpCodeCompleted", f"Opcode {op:#x} completed")
        )

    def halted(self, end: str) -> int | None:
        """No probe: flat."""
        return 0

    def openocd(self, end: str, line: str, wait: float = 2.0) -> str:
        """No probe."""
        return ""

    def start_rtt_shell(self) -> None:
        """No probe."""

    def shell(self, end: str, line: str, wait: float = 3.0) -> str:
        """A canned `kernel thread list` whose cycles grow with the virtual clock."""
        c = int(self.clock * 1000)
        return (
            f" 0x20001000 uspRadio\n\tTotal execution cycles: {c} (1 %)\n"
            f" 0x20002000 idle\n\tTotal execution cycles: {c * 99} (99 %)\n"
        )

    def inject(self, frames: int, size: int, pace: float) -> int:
        """Bytes are counted as written; nothing reaches the radio."""
        self.sleep(frames * pace)
        return frames * size

    @contextlib.contextmanager
    def flood(self, size: int, pace: float) -> Iterator[dict]:
        """No tty."""
        yield {"written": 0}

    @contextlib.contextmanager
    def capture(self, label: str) -> Iterator[dict]:
        """No tty."""
        yield {"bytes": 0, "path": "dry-run"}


# --------------------------------------------------------------------------
# Drills and the runner


@dataclass(frozen=True)
class Drill:
    """One drill from the #593 table.

    build: 'overlay' (the build under test), 'plain' (D1s), checked against
      the flight ELF. same_pins: both ends must share fprime-zephyr and usp
      pins (every drill but D6). keys: bench keys beyond each end's basics.
    """

    id: str
    title: str
    ends: tuple[str, ...]
    run: Callable[[Rig], dict]
    criteria: tuple[Criterion, ...]
    build: str = "overlay"
    same_pins: bool = True
    keys: tuple[str, ...] = ()


def preflight_checks(rig: Rig, drill: Drill) -> list[Check]:
    """Build and pin checks that make a run comparable across the suite."""
    checks = []
    if "flight" in drill.ends and drill.build in ("overlay", "plain"):
        has = elf_has_overlay(rig.bench.values.get("FLIGHT_ELF", ""))
        want = drill.build == "overlay"
        checks.append(
            Check(
                f"flight build is {drill.build}",
                has == want,
                f"ELF has SEGGER RTT: {has}",
            )
        )
    if drill.same_pins and {"flight", "grc"} <= set(drill.ends):
        fz = {e: rig.dicts[e].library_sha("fprime-zephyr") for e in ("flight", "grc")}
        checks.append(
            Check("both ends same fprime-zephyr", len(set(fz.values())) == 1, str(fz))
        )
        pins = {
            e: module_pins(rig.bench.values.get(f"{e.upper()}_REPO", ""))
            for e in ("flight", "grc")
        }
        checks.append(
            Check("both ends same usp pins", len(set(pins.values())) == 1, str(pins))
        )
    return checks


def run_drill(
    drill: Drill, bench: Bench, log_root: str, params: dict[str, str], dry: bool
) -> bool:
    """Run one drill with the universal gate and print its evidence block."""
    stamp = datetime.datetime.now()
    run_dir = (
        Path(log_root)
        / f"{stamp:%Y-%m-%d}-drills"
        / f"{stamp:%H%M%S}-{drill.id}{'-dry' if dry else ''}"
    )
    run_dir.mkdir(parents=True, exist_ok=True)
    problems = bench.problems(drill.ends, drill.keys)
    if problems and not dry:
        raise BenchError(
            "\n  ".join([f"{drill.id} cannot run on this bench:", *problems])
        )
    rig = (DryRig if dry else Rig)(bench, drill.ends, run_dir, params)
    if dry:
        rig.notes.append(
            "DRY RUN: fake bench on a virtual clock. Verdicts are not evidence."
        )
        rig.notes += [f"bench gap: {p}" for p in problems]
    rig.log(f"=== {drill.id}: {drill.title} (bench {bench.path}, logs {run_dir})")
    provenance, checks = rig.provenance()
    checks += preflight_checks(rig, drill)
    rig.settle()
    before = rig.snapshot()
    measurements: dict = {}
    try:
        measurements = drill.run(rig)
    except DrillAbort as e:
        rig.notes.append(f"ABORT: {e}")
        rig.log(f"ABORT: {e}")
    rig.settle()
    after = rig.snapshot()
    checks += universal_gate(before, after, rig.tally) + evaluate(
        drill.criteria, measurements
    )
    (run_dir / "measurements.json").write_text(
        json.dumps(measurements, indent=2, default=str)
    )
    for end, log in rig.logs.items():
        log.archive(run_dir, end)
    text = render_evidence(
        drill.id,
        drill.title,
        provenance,
        before,
        after,
        checks,
        rig.notes,
        str(run_dir),
    )
    (run_dir / "evidence.md").write_text(text)
    print("\n" + text, flush=True)
    return verdict(checks)


def parse_args(drills: list[Drill], argv: list[str] | None) -> argparse.Namespace:
    """Common command line for every drill entry point."""
    ids = [d.id for d in drills]
    p = argparse.ArgumentParser(
        description=f"Bench drills {', '.join(ids)} (issue #593 protocol).",
        epilog="Exit status: 0 all PASS (or dry run), 1 a drill FAILED, 2 the bench file cannot run it.",
    )
    p.add_argument(
        "drill",
        nargs="*",
        choices=ids,
        help=f"drills to run (default: {' '.join(ids)})",
    )
    p.add_argument(
        "--bench",
        type=Path,
        default=DEFAULT_BENCH,
        help="bench env file (default: %(default)s)",
    )
    p.add_argument(
        "--log-root",
        help=f"raw log root (default: bench LOG_ROOT, else {DEFAULT_LOG_ROOT}; /tmp/bench-drills for --dry-run)",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="run the drill logic offline against a fake bench",
    )
    p.add_argument(
        "--param",
        action="append",
        default=[],
        metavar="KEY=VALUE",
        help="drill parameter override",
    )
    return p.parse_args(argv)


def main(drills: list[Drill], argv: list[str] | None = None) -> int:
    """Entry point shared by every drill script and run_all."""
    args = parse_args(drills, argv)
    selected = [d for d in drills if not args.drill or d.id in args.drill]
    try:
        bench = Bench(load_bench(args.bench))
        default_root = (
            "/tmp/bench-drills"
            if args.dry_run
            else bench.values.get("LOG_ROOT") or DEFAULT_LOG_ROOT
        )
        log_root = args.log_root or default_root
        params = dict(kv.split("=", 1) for kv in args.param)
        results = [
            (d.id, run_drill(d, bench, log_root, params, args.dry_run))
            for d in selected
        ]
    except BenchError as e:
        print(f"BENCH ERROR: {e}", file=sys.stderr)
        return 2
    print("| Drill | Verdict |\n|---|---|")
    for drill_id, ok in results:
        print(f"| {drill_id} | {'PASS' if ok else 'FAIL'} |")
    return 0 if args.dry_run or all(ok for _, ok in results) else 1
