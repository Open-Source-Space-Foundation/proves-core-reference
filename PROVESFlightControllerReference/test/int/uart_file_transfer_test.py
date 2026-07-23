"""
uart_file_transfer_test.py:

Reproduces GitHub issue #457 (UART file uplink/downlink fails for files
larger than ~1 chunk; downlinks emit 0-byte packets).

Uplink oracle: fileManager.CalculateCrc on-board, compared against
zlib.crc32(data) ^ 0xFFFFFFFF computed locally over the source bytes.

Downlink verification: command FileHandling.fileDownlink.SendFile for a file
already known-good on the board (uplinked and CRC-verified in the same test),
then compare the bytes written by the GDS's own FileDownlinker against the
original bytes.

Chunk size for uplink is fixed project-wide in fprime-gds.yml
(file-uplink-chunk-size: 204), so "N chunks" here means N * 204 bytes.
"""

import random
import time
import zlib
from pathlib import Path

import pytest
from fprime_gds.common.files.helpers import FileStates
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# Needs only a bare flight controller: exercises the UART command/file paths
# and the SD filesystem, no face/antenna/battery hardware involved.
pytestmark = [pytest.mark.board_only]

UPLINK_CHUNK_SIZE = 204  # from fprime-gds.yml: file-uplink-chunk-size

FILE_MANAGER = "FileHandling.fileManager"
FILE_DOWNLINK = "FileHandling.fileDownlink"


def _make_random_file(tmp_path: Path, num_bytes: int, name: str) -> Path:
    """Create a file of exactly num_bytes of pseudo-random data."""
    p = tmp_path / name
    rng = random.Random(1234 + num_bytes)  # deterministic per-size for reproducibility
    p.write_bytes(bytes(rng.getrandbits(8) for _ in range(num_bytes)))
    return p


def _local_crc(data: bytes) -> int:
    """Matches fileManager.CalculateCrc on-board oracle (see project memory:
    fprime-crc-verification.md)."""
    return zlib.crc32(data) ^ 0xFFFFFFFF


def _wait_for_uplink_idle(uplinker, timeout_s: float) -> bool:
    """Poll the GDS-side uplinker until its state machine returns to IDLE
    (transfer finished, successfully or not -- see FileUplinker.finish()/
    data_callback() in fprime_gds.common.files.uplinker). NOTE:
    current_files()/queue.current() is NOT usable for this: UplinkQueue
    appends to an unbounded __file_store history and never removes entries,
    so it is never empty even long after a transfer completes."""
    deadline = time.time() + timeout_s
    # Give the queue thread a moment to pick up the enqueued file and leave IDLE.
    time.sleep(0.5)
    while time.time() < deadline:
        if uplinker.state == FileStates.IDLE:
            return True
        time.sleep(0.25)
    return False


def _uplink_and_verify_crc(
    fprime_test_api: IntegrationTestAPI,
    local_path: Path,
    dest_path: str,
    timeout_s: float,
):
    """Uplink local_path to dest_path on the board, then verify via
    CalculateCrc oracle. Returns (crc_event_seen, board_crc_or_None)."""
    data = local_path.read_bytes()
    expected_crc = _local_crc(data)

    uplinker = fprime_test_api.pipeline.files.uplinker
    fprime_test_api.clear_histories()
    uplinker.enqueue(str(local_path), dest_path)

    idle = _wait_for_uplink_idle(uplinker, timeout_s)

    fprime_test_api.clear_histories()
    fprime_test_api.send_command(f"{FILE_MANAGER}.CalculateCrc", [dest_path])
    evt = fprime_test_api.await_event(
        f"{FILE_MANAGER}.CalculateCrcSucceeded", timeout=15
    )
    fail_evt = None
    if evt is None:
        fail_evt = fprime_test_api.await_event(
            f"{FILE_MANAGER}.CalculateCrcFailed", timeout=1
        )

    return {
        "uplink_idle": idle,
        "crc_event": evt,
        "crc_fail_event": fail_evt,
        "expected_crc": expected_crc,
        "size": len(data),
    }


@pytest.mark.parametrize("n_chunks", [1, 3, 5])
def test_uplink_n_chunks(
    fprime_test_api: IntegrationTestAPI, start_gds, tmp_path, n_chunks
):
    """Uplink a file of n_chunks * UPLINK_CHUNK_SIZE bytes and verify its
    on-board CRC matches the local CRC32 oracle."""
    size = n_chunks * UPLINK_CHUNK_SIZE
    local_path = _make_random_file(tmp_path, size, f"uplink_{n_chunks}c.bin")
    dest = f"/uplink_test_{n_chunks}c.bin"

    result = _uplink_and_verify_crc(
        fprime_test_api, local_path, dest, timeout_s=10 + n_chunks * 3
    )

    print(
        f"[n_chunks={n_chunks}] size={result['size']} "
        f"uplink_idle={result['uplink_idle']} "
        f"crc_event={result['crc_event']} "
        f"crc_fail_event={result['crc_fail_event']} "
        f"expected_crc=0x{result['expected_crc']:08x}"
    )

    assert result["uplink_idle"], (
        f"uplink queue did not go idle within timeout for {n_chunks} chunks "
        f"({size} bytes) -- uplink likely hung"
    )
    assert result["crc_fail_event"] is None, (
        f"on-board CalculateCrc explicitly failed: {result['crc_fail_event']}"
    )
    assert result["crc_event"] is not None, (
        f"no CalculateCrcSucceeded event received for {n_chunks} chunks "
        f"({size} bytes) -- file likely missing/empty on board"
    )
    board_crc = result["crc_event"].args[1].val
    assert board_crc == result["expected_crc"], (
        f"CRC mismatch for {n_chunks} chunks: board=0x{board_crc:08x} "
        f"expected=0x{result['expected_crc']:08x}"
    )


@pytest.mark.slow
@pytest.mark.parametrize("n_chunks", [1000])
def test_uplink_large(
    fprime_test_api: IntegrationTestAPI, start_gds, tmp_path, n_chunks
):
    """Large uplink case -- skipped by default (deselect with -m 'not slow')."""
    size = n_chunks * UPLINK_CHUNK_SIZE
    local_path = _make_random_file(tmp_path, size, f"uplink_{n_chunks}c.bin")
    dest = f"/uplink_test_{n_chunks}c.bin"

    result = _uplink_and_verify_crc(
        fprime_test_api, local_path, dest, timeout_s=60 + n_chunks * 0.5
    )
    assert result["uplink_idle"]
    assert result["crc_event"] is not None
    board_crc = result["crc_event"].args[1].val
    assert board_crc == result["expected_crc"]


@pytest.mark.slow
@pytest.mark.xfail(
    reason="issue #471: comms buffer pool leak FATALs the board on the second "
    "large uplink of a boot; remove this marker as the #471 acceptance gate",
    strict=False,
)
def test_large_round_trip(fprime_test_api: IntegrationTestAPI, start_gds, tmp_path):
    """~204KB (1000-chunk) uplink + downlink round trip.

    Uplinks a single large file, verifies it on-board via the CRC oracle, then
    downlinks it back and compares bytes. This protects the full UART file
    transfer path end-to-end (issues #457 and #461).

    Deliberately avoids send_and_assert_command: the board occasionally emits
    EVR timestamps out of order (e.g. OpCodeDispatched stamped at .999 of the
    prior second), which makes the test API's chronological sequence search
    fail even though the on-board operation succeeded.
    """
    n_chunks = 1000
    size = n_chunks * UPLINK_CHUNK_SIZE
    local_path = _make_random_file(tmp_path, size, "round_trip_large.bin")
    # The Uplinker deletes its source file on success; snapshot the bytes now.
    original_bytes = local_path.read_bytes()
    board_path = "/round_trip_large.bin"
    dest_name = "round_trip_large_received.bin"

    print(f"[round-trip] uplinking {size} bytes as one file...")
    t0 = time.time()
    up = _uplink_and_verify_crc(fprime_test_api, local_path, board_path, timeout_s=900)
    t_up = time.time() - t0
    assert up["uplink_idle"], f"{size}-byte uplink did not go idle within 900s"
    assert up["crc_event"] is not None, (
        "no CalculateCrcSucceeded after large uplink -- file missing/empty"
    )
    board_crc = up["crc_event"].args[1].val
    assert board_crc == up["expected_crc"], (
        f"large uplink corrupted: board=0x{board_crc:08x} "
        f"expected=0x{up['expected_crc']:08x}"
    )
    print(f"[round-trip] uplink OK in {t_up:.1f}s ({size / t_up:.1f} B/s)")

    downlinker = fprime_test_api.pipeline.files.downlinker
    fprime_test_api.clear_histories()
    t0 = time.time()
    fprime_test_api.send_command(f"{FILE_DOWNLINK}.SendFile", [board_path, dest_name])

    candidate = Path(downlinker._FileDownlinker__directory) / dest_name
    deadline = time.time() + 900
    landed = False
    while time.time() < deadline:
        if candidate.exists() and candidate.stat().st_size == size:
            landed = True
            break
        time.sleep(1)
    t_dl = time.time() - t0
    actual_size = candidate.stat().st_size if candidate.exists() else 0
    print(
        f"[round-trip] downlink landed={landed} elapsed={t_dl:.1f}s "
        f"size={actual_size}/{size}"
    )
    assert landed, f"{size}-byte downlink did not complete within 900s"
    assert candidate.read_bytes() == original_bytes, (
        "downlinked bytes do not match the uplinked source"
    )
    print(f"[round-trip] SUCCESS: up {size / t_up:.1f} B/s, down {size / t_dl:.1f} B/s")


@pytest.mark.parametrize("n_chunks", [1, 3, 5])
def test_downlink_n_chunks(
    fprime_test_api: IntegrationTestAPI, start_gds, tmp_path, n_chunks
):
    """Uplink a known-good file (verified via CRC oracle), then downlink it
    back and compare the bytes the ground actually received."""
    size = n_chunks * UPLINK_CHUNK_SIZE
    local_path = _make_random_file(tmp_path, size, f"downlink_src_{n_chunks}c.bin")
    # NOTE: the fprime_gds Uplinker deletes its source file on successful completion
    # (FileUplinker.finish() -> os.remove(self.active.source)), so local_path will no
    # longer exist after _uplink_and_verify_crc() returns. Snapshot the original bytes
    # now for the post-downlink comparison below.
    original_bytes = local_path.read_bytes()
    board_path = f"/downlink_test_{n_chunks}c.bin"
    dest_name = f"DL_{n_chunks}c.bin"

    up = _uplink_and_verify_crc(
        fprime_test_api, local_path, board_path, timeout_s=10 + n_chunks * 3
    )
    assert up["uplink_idle"], "setup uplink for downlink test did not finish"
    assert up["crc_event"] is not None, (
        "setup uplink for downlink test failed CRC check"
    )
    assert up["crc_event"].args[1].val == up["expected_crc"], (
        "setup uplink for downlink test produced wrong CRC on-board -- "
        "cannot trust downlink comparison"
    )

    downlinker = fprime_test_api.pipeline.files.downlinker
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(f"{FILE_DOWNLINK}.SendFile", [board_path, dest_name])

    # Poll for the downlinked file to land in the GDS storage directory with
    # the expected size, or for a timeout.
    deadline = time.time() + (10 + n_chunks * 5)
    received_path = None
    while time.time() < deadline:
        candidate = Path(downlinker._FileDownlinker__directory) / dest_name
        if candidate.exists() and candidate.stat().st_size == size:
            received_path = candidate
            break
        time.sleep(0.5)

    print(
        f"[downlink n_chunks={n_chunks}] size={size} "
        f"received_path={received_path} "
        f"exists={(Path(downlinker._FileDownlinker__directory) / dest_name).exists()} "
        f"actual_size={(Path(downlinker._FileDownlinker__directory) / dest_name).stat().st_size if (Path(downlinker._FileDownlinker__directory) / dest_name).exists() else 'N/A'}"
    )

    assert received_path is not None, (
        f"downlink of {n_chunks} chunks ({size} bytes) did not complete/"
        f"arrive at expected size within timeout -- see printed diagnostics"
    )
    received_bytes = received_path.read_bytes()
    assert received_bytes == original_bytes, (
        f"downlinked bytes for {n_chunks} chunks do not match source "
        f"(len received={len(received_bytes)}, len original={len(original_bytes)})"
    )
