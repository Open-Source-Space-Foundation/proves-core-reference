"""
fs_concurrency_test.py:

Guards the SD-card FatFs volume against concurrent use from several F Prime
threads, the failure seen on Orange EM and on Electra during file uplink: the
root directory was rewritten as a garbage 0-byte entry, every other root file
and subdirectory vanished, and long-name uploads into the damaged root were
acknowledged but lost. The bench repro (2026-10-29) flooded NO_OP uplinks every
20 ms mixed with fileManager FileSize/CalculateCrc/ListDirectory reads and wiped
the root twice in about 1,080 such commands, with no reboot.

The same load can also stop the flight software outright: with the FatFs volume
lock enabled the bench control image stopped answering commands and stopped
sending telemetry about 40 s into the repro. So this test reports two distinct
failures:

  - "root directory corrupted": a sentinel or root entry went missing, an
    unexpected entry appeared, a CRC changed, or an uplinked/downlinked file
    does not match its source.
  - "FSW stopped responding": a command went unanswered for CMD_SILENCE_S or
    no telemetry arrived for TLM_SILENCE_S.

Load, run concurrently for --fs-flood-commands commands (default 2,000):
  - the repro's arm B mix: 60% CMD_NO_OP at a 20 ms gap, 40% fileManager
    FileSize/CalculateCrc/ListDirectory on the sentinels at a 50 ms gap;
  - back-to-back file uplinks to long names in the root on the fileUplink
    thread, the operational trigger;
  - a fileDownlink.SendFile of a sentinel every DOWNLINK_EVERY commands.
At most WINDOW commands are outstanding, so the flood runs as fast as the board
answers without overrunning the UART receive path.

Why 2,000 commands: the bench rate was 2 wipes in 1,080 arm B commands, about
1 in 540. If wipes arrive independently at that rate, a broken build survives
2,000 commands with probability (1 - 1/540)^2000 = 2.5%, so the test fails it
about 98% of the time. The rate rests on two events (95% interval for the
mean: 1 in 150 to 1 in 4,500) and was measured on an image that rewrote
//sequence_number.txt on every uplinked command; main persists it at 1 Hz, so
the continuous uplinks here supply the competing writer. Recalibrate the count
from a negative-control run before relying on the detection probability: build
without the FatFs volume lock with
  fprime-util generate --force -DEXTRA_CONF_FILE=$PWD/PROVESFlightControllerReference/test/int/fatfs-no-lock.conf
then run this test with a larger --fs-flood-commands.

The root is listed as "//": the Zephyr VFS lists mount points for "/", so only
"//" shows the SD card's root entries.
"""

import random
import time
import zlib
from dataclasses import dataclass, field
from pathlib import Path

import pytest
from fprime_gds.common.files.helpers import FileStates
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

pytestmark = [pytest.mark.fs_concurrency, pytest.mark.uart_only]

FILE_MANAGER = "FileHandling.fileManager"
FILE_DOWNLINK = "FileHandling.fileDownlink"
FILE_UPLINK = "FileHandling.fileUplink"
CMD_DISPATCH = "CdhCore.cmdDisp"
NOOP = f"{CMD_DISPATCH}.CMD_NO_OP"

ROOT = "//"
SUBDIR = "/fsc_dir"
SENTINELS = [
    "/fsc0.bin",
    "/fsc_sentinel_long_name_1.bin",
    f"{SUBDIR}/fsc2.bin",
    f"{SUBDIR}/fsc_sentinel_long_name_3.bin",
]
UPLINK_PATH = "/fsc_uplink_long_name_{}.bin"
UPLINK_BYTES = 4 * 204
MAX_UPLINKS = 12
DOWNLINK_SOURCE = SENTINELS[1]
DOWNLINK_NAME = "fsc_down_{}.bin"
DOWNLINK_EVERY = 200
CHECK_EVERY = 250

WINDOW = 8
NOOP_GAP_S = 0.02
READ_GAP_S = 0.05
CMD_SILENCE_S = 15.0
TLM_SILENCE_S = 20.0
LISTING_TIMEOUT_S = 20.0


def sentinel_bytes(path: str) -> bytes:
    """Content derived from the path, so each sentinel is distinct and reproducible."""
    return (path.encode() * 64)[:512]


def uplink_bytes(index: int) -> bytes:
    """Deterministic pseudo-random content for the index-th flood uplink."""
    rng = random.Random(f"fsc-uplink-{index}")
    return bytes(rng.getrandbits(8) for _ in range(UPLINK_BYTES))


def board_crc(data: bytes) -> int:
    """fileManager.CalculateCrc reports the one's complement of zlib.crc32."""
    return zlib.crc32(data) ^ 0xFFFFFFFF


class FswUnresponsive(AssertionError):
    """The flight software stopped answering commands or sending telemetry."""


class RootCorrupted(AssertionError):
    """A root entry, sentinel, or transferred file did not survive the flood."""


@dataclass
class Listing:
    """Result of one ListDirectory: entry names (subdirectories end in '/') and the board's count."""

    ok: bool
    names: set[str] = field(default_factory=set)
    reported: int | None = None


class Board:
    """Command traffic with liveness tracking.

    Counts CmdDispatcher completions (OpCodeCompleted/OpCodeError) against
    commands sent, and the arrival time of telemetry, so every wait doubles as
    a liveness check.
    """

    def __init__(self, api: IntegrationTestAPI):
        self.api = api
        self.events = api.get_event_subhistory(fsw_order=False)
        self.telemetry = api.get_telemetry_subhistory(fsw_order=False)
        self.log: list[tuple[str, list]] = []
        self.sent = 0
        self.completed = 0
        self.last_completion = time.time()
        self.last_telemetry = time.time()

    def close(self):
        """Detach the event and telemetry subhistories."""
        self.api.remove_event_subhistory(self.events)
        self.api.remove_telemetry_subhistory(self.telemetry)

    @property
    def outstanding(self) -> int:
        """Commands sent that have no completion yet."""
        return self.sent - self.completed

    def pump(self):
        """Consume new events and telemetry, then check liveness."""
        now = time.time()
        for evt in self.events.retrieve_new():
            name = evt.get_template().get_full_name()
            self.log.append((name, [arg.val for arg in evt.args or []]))
            if name in (
                f"{CMD_DISPATCH}.OpCodeCompleted",
                f"{CMD_DISPATCH}.OpCodeError",
            ):
                self.completed += 1
                self.last_completion = now
        if self.telemetry.retrieve_new():
            self.last_telemetry = now
        self.check_alive()

    def check_alive(self):
        """Raise FswUnresponsive when commands or telemetry have gone silent."""
        now = time.time()
        if self.outstanding > 0 and now - self.last_completion > CMD_SILENCE_S:
            raise FswUnresponsive(
                f"FSW stopped responding: {self.outstanding} of {self.sent} commands "
                f"unanswered for {now - self.last_completion:.0f} s; "
                f"recent events: {self.recent_warnings()}"
            )
        if now - self.last_telemetry > TLM_SILENCE_S:
            raise FswUnresponsive(
                f"FSW stopped responding: no telemetry for {now - self.last_telemetry:.0f} s "
                f"after {self.sent} commands; recent events: {self.recent_warnings()}"
            )

    def recent_warnings(self, n: int = 8) -> list[str]:
        """The last n error or failure events, for failure messages."""
        hits = [
            f"{name}{args}"
            for name, args in self.log
            if "Error" in name or "Fail" in name
        ]
        return hits[-n:]

    def wait_window(self, limit: int):
        """Block until fewer than limit commands are outstanding."""
        while self.outstanding >= limit:
            time.sleep(0.01)
            self.pump()

    def send(self, command: str, args: list[str] | None = None, gap: float = 0.0):
        """Send within the window, then pause for gap seconds."""
        self.wait_window(WINDOW)
        self.api.send_command(command, args or [])
        if self.outstanding == 0:
            self.last_completion = time.time()
        self.sent += 1
        time.sleep(gap)
        self.pump()

    def drain(self):
        """Block until every sent command has completed."""
        self.wait_window(1)

    def await_event(self, start: int, name: str, match, timeout: float):
        """Return the args of the first event named name at or after log index start that satisfies match."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.pump()
            for evt_name, args in self.log[start:]:
                if evt_name == name and match(args):
                    return args
            time.sleep(0.05)
        return None

    def listing(self, path: str) -> Listing:
        """List path and collect its entries from the listing events."""
        start = len(self.log)
        self.send(f"{FILE_MANAGER}.ListDirectory", [path])
        deadline = time.time() + LISTING_TIMEOUT_S
        while time.time() < deadline:
            self.pump()
            names, done = set(), None
            for name, args in self.log[start:]:
                if not args or args[0] != path:
                    continue
                if name == f"{FILE_MANAGER}.DirectoryListing":
                    names.add(args[1])
                elif name == f"{FILE_MANAGER}.DirectoryListingSubdir":
                    names.add(args[1] + "/")
                elif name == f"{FILE_MANAGER}.ListDirectorySucceeded":
                    done = Listing(True, names, args[1])
                elif name == f"{FILE_MANAGER}.ListDirectoryError":
                    done = Listing(False)
            if done is not None:
                return done
            time.sleep(0.05)
        return Listing(False)

    def stable_listing(self, path: str) -> Listing:
        """List twice if the entry events do not add up to the reported count,
        so a dropped event is not read as a corrupted directory."""
        for _ in range(2):
            result = self.listing(path)
            if not result.ok or result.reported == len(result.names):
                return result
        return result

    def crc(self, path: str) -> int | None:
        """The board's CRC of path, or None if the calculation failed."""
        start = len(self.log)
        self.send(f"{FILE_MANAGER}.CalculateCrc", [path])
        args = self.await_event(
            start,
            f"{FILE_MANAGER}.CalculateCrcSucceeded",
            lambda a: a[0] == path,
            timeout=CMD_SILENCE_S,
        )
        return None if args is None else int(args[1])


class Uplinks:
    """Keeps one file uplink in flight to a new long root name until MAX_UPLINKS."""

    def __init__(self, api: IntegrationTestAPI, tmp_path: Path):
        self.uplinker = api.pipeline.files.uplinker
        self.tmp_path = tmp_path
        self.started: list[str] = []
        self.enqueued_at = 0.0

    def tick(self):
        """Start the next uplink once the previous one has finished."""
        if len(self.started) >= MAX_UPLINKS:
            return
        if self.started and (
            self.uplinker.state != FileStates.IDLE
            or time.time() - self.enqueued_at < 1.0
        ):
            return
        index = len(self.started)
        local = self.tmp_path / f"fsc_uplink_{index}.bin"
        local.write_bytes(uplink_bytes(index))
        dest = UPLINK_PATH.format(index)
        self.uplinker.enqueue(str(local), dest)
        self.enqueued_at = time.time()
        self.started.append(dest)

    def wait_idle(self, board: Board, timeout: float = 60.0):
        """Let the last uplink finish while keeping the liveness checks running."""
        deadline = time.time() + timeout
        time.sleep(1.0)
        while time.time() < deadline and self.uplinker.state != FileStates.IDLE:
            board.pump()
            time.sleep(0.1)


def uplink_file(
    api: IntegrationTestAPI, board: Board, tmp_path: Path, dest: str, data: bytes
):
    """Uplink data to dest and wait for the board to report it received."""
    local = tmp_path / dest.strip("/").replace("/", "__")
    local.write_bytes(data)
    start = len(board.log)
    api.pipeline.files.uplinker.enqueue(str(local), dest)
    received = board.await_event(
        start, f"{FILE_UPLINK}.FileReceived", lambda a: a[0] == dest, timeout=60
    )
    assert received is not None, f"setup: uplink of {dest} not received"


def ground_file_matches(
    board: Board, local: Path, data: bytes, timeout: float = 10.0
) -> bool:
    """The GDS writes the ground copy as file packets arrive, which can trail FileSent."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if local.is_file() and local.read_bytes() == data:
            return True
        board.pump()
        time.sleep(0.2)
    return False


def compare_root(baseline: set[str], allowed: set[str], current: Listing) -> list[str]:
    """Problems with current against the baseline names; allowed names may appear."""
    if not current.ok:
        return [f"{ROOT} listing failed"]
    problems = []
    missing = baseline - current.names
    unexpected = current.names - baseline - allowed
    if missing:
        problems.append(f"{ROOT} missing {sorted(missing)}")
    if unexpected:
        problems.append(f"{ROOT} unexpected {sorted(unexpected)}")
    return problems


def file_name(path: str) -> str:
    """The last path component."""
    return path.rsplit("/", 1)[1]


@pytest.fixture
def board(fprime_test_api: IntegrationTestAPI, start_gds):
    """A Board bound to this test's API."""
    b = Board(fprime_test_api)
    yield b
    b.close()


@pytest.fixture
def cleanup(fprime_test_api: IntegrationTestAPI, board: Board):
    """Best-effort removal of every file the test created, on the board and on ground."""
    created: list[str] = []
    downlinked: list[Path] = []
    yield created, downlinked
    for path in created:
        fprime_test_api.send_command(f"{FILE_MANAGER}.RemoveFile", [path, "true"])
        time.sleep(0.3)
    fprime_test_api.send_command(f"{FILE_MANAGER}.RemoveDirectory", [SUBDIR])
    time.sleep(1.0)
    for local in downlinked:
        local.unlink(missing_ok=True)


def test_root_survives_concurrent_file_traffic(
    fprime_test_api: IntegrationTestAPI,
    board: Board,
    cleanup,
    tmp_path: Path,
    request: pytest.FixtureRequest,
):
    """Flood commands and file reads during file uplink and downlink; the root
    directory, sentinels, and transferred files must survive, and the FSW must
    keep answering."""
    created, downlinked = cleanup
    flood_commands = request.config.getoption("--fs-flood-commands")
    rng = random.Random(20261029)

    board.send(f"{FILE_MANAGER}.CreateDirectory", [SUBDIR])
    board.drain()
    for path in SENTINELS:
        created.append(path)
        uplink_file(fprime_test_api, board, tmp_path, path, sentinel_bytes(path))
    expected_crc = {p: board_crc(sentinel_bytes(p)) for p in SENTINELS}
    for path, crc in expected_crc.items():
        assert board.crc(path) == crc, (
            f"setup: sentinel {path} CRC mismatch after uplink"
        )

    baseline = board.stable_listing(ROOT)
    assert baseline.ok, f"setup: {ROOT} listing failed"
    root_sentinels = {file_name(p) for p in SENTINELS if p.count("/") == 1} | {
        file_name(SUBDIR) + "/"
    }
    assert root_sentinels <= baseline.names, (
        f"setup: sentinels missing from {ROOT}: {sorted(baseline.names)}"
    )
    uplink_names = {file_name(UPLINK_PATH.format(i)) for i in range(MAX_UPLINKS)}
    baseline_names = baseline.names - uplink_names

    uplinks = Uplinks(fprime_test_api, tmp_path)
    downlinks: list[str] = []
    read_targets = SENTINELS + [ROOT, SUBDIR]

    def check_root(when: str):
        problems = compare_root(
            baseline_names, uplink_names, board.stable_listing(ROOT)
        )
        if problems:
            raise RootCorrupted(f"root directory corrupted {when}: {problems}")

    try:
        for i in range(flood_commands):
            uplinks.tick()
            if i % DOWNLINK_EVERY == DOWNLINK_EVERY // 2:
                name = DOWNLINK_NAME.format(len(downlinks))
                downlinks.append(name)
                board.send(f"{FILE_DOWNLINK}.SendFile", [DOWNLINK_SOURCE, name])
            elif rng.random() < 0.6:
                board.send(NOOP, gap=NOOP_GAP_S)
            else:
                target = rng.choice(read_targets)
                if target in (ROOT, SUBDIR):
                    board.send(
                        f"{FILE_MANAGER}.ListDirectory", [target], gap=READ_GAP_S
                    )
                else:
                    kind = rng.choice(["FileSize", "CalculateCrc"])
                    board.send(f"{FILE_MANAGER}.{kind}", [target], gap=READ_GAP_S)
            if i and i % CHECK_EVERY == 0:
                board.drain()
                check_root(f"after {i} commands")

        board.drain()
        uplinks.wait_idle(board)
        check_root(f"after {flood_commands} commands")

        problems = []
        for path, crc in expected_crc.items():
            actual = board.crc(path)
            if actual != crc:
                problems.append(f"sentinel {path} CRC {actual} != {crc}")
        for index, dest in enumerate(uplinks.started):
            actual = board.crc(dest)
            if actual != board_crc(uplink_bytes(index)):
                problems.append(
                    f"uplinked {dest} CRC {actual} != {board_crc(uplink_bytes(index))}"
                )
        down_dir = Path(fprime_test_api.pipeline.files.downlinker.directory)
        for name in downlinks:
            sent = board.await_event(
                0,
                f"{FILE_DOWNLINK}.FileSent",
                lambda a, n=name: a[1] == n,
                timeout=CMD_SILENCE_S,
            )
            local = down_dir / name
            downlinked.append(local)
            if sent is None:
                problems.append(f"downlink {name}: no FileSent")
            elif not ground_file_matches(board, local, sentinel_bytes(DOWNLINK_SOURCE)):
                problems.append(
                    f"downlink {name}: ground copy at {local} differs from {DOWNLINK_SOURCE}"
                )
        if problems:
            raise RootCorrupted(
                f"root directory corrupted after {flood_commands} commands: {problems}"
            )
    finally:
        created.extend(uplinks.started)

    print(
        f"[fs_concurrency] {board.sent} commands, {len(uplinks.started)} uplinks, "
        f"{len(downlinks)} downlinks, root intact: {sorted(baseline_names)}"
    )
