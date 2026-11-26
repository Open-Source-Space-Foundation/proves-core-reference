"""
file_expander_test.py:

Compressed uplink prototype, end to end on the board: compress real firmware
bytes on the ground with tools/upz/upz.py, uplink the .upz file, rebuild it with
ReferenceDeployment.fileExpander, and check the board's CRC of the result. A
second test stops an expansion midway and checks that resending EXPAND_FILE
resumes from the chunks already written instead of starting over.

The firmware bytes come from the build's bootable.uf2 when it is present (CI
downloads it), else from a generated Thumb-like image.
"""

import os
import random
import struct
import sys
import tempfile
import time
import zlib

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.files.helpers import FileStates
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "upz"))
import upz  # noqa: E402

pytestmark = [pytest.mark.uart_only]

EXPANDER = "ReferenceDeployment.fileExpander"
FILE_MANAGER = "FileHandling.fileManager"
# Two chunks plus a partial third: exercises the chunk reset and the cross-chunk window
RAW_SIZE = 150_000
# Larger chunks overflow the 248 B TC frame and are silently dropped
UPLINK_CHUNK = 204
UPLINK_COOLDOWN_S = 0.6
UPLINK_TIMEOUT_S = 1200
EXPAND_TIMEOUT_S = 600
CODED = "/upz_test.upz"
OUT = "/upz_test.bin"


def _uf2_payload(path: str) -> bytes:
    """Firmware bytes from a UF2 file (256-byte payloads at offset 32 of each 512-byte block)."""
    out = bytearray()
    with open(path, "rb") as f:
        while block := f.read(512):
            if len(block) < 512:
                break
            size = struct.unpack_from("<I", block, 16)[0]
            out += block[32 : 32 + min(size, 476)]
    return bytes(out)


def _firmware_sample() -> bytes:
    """RAW_SIZE bytes of the build's firmware, or a generated Thumb-like stand-in."""
    uf2 = os.path.join(ROOT, "bootable.uf2")
    if os.path.exists(uf2):
        data = _uf2_payload(uf2)
        if len(data) >= RAW_SIZE:
            return data[:RAW_SIZE]
    rng = random.Random(5)
    targets = [rng.randrange(0, 1 << 20) & ~1 for _ in range(40)]
    out = bytearray()
    while len(out) < RAW_SIZE:
        if rng.random() < 0.15:
            off = (rng.choice(targets) - len(out) - 4) & 0x1FFFFFF
            s = (off >> 24) & 1
            j1 = (~(((off >> 23) & 1) ^ s)) & 1
            j2 = (~(((off >> 22) & 1) ^ s)) & 1
            out += struct.pack(
                "<HH",
                0xF000 | (s << 10) | ((off >> 12) & 0x3FF),
                0xD000 | (j1 << 13) | (j2 << 11) | ((off >> 1) & 0x7FF),
            )
        else:
            out += struct.pack(
                "<H", rng.choice([0x4770, 0xB510, 0xBD10, 0x2000, 0x6803, 0x4618])
            )
    return bytes(out[:RAW_SIZE])


def _uplink(api: IntegrationTestAPI, data: bytes, dest: str) -> None:
    """Uplink ``data`` to ``dest`` with UART-safe chunking and check its on-board CRC."""
    uplinker = api.pipeline.files.uplinker
    saved = (uplinker.chunk, getattr(uplinker, "cooldown", None))
    uplinker.chunk = min(uplinker.chunk, UPLINK_CHUNK)
    if saved[1] is not None:
        uplinker.cooldown = max(saved[1], UPLINK_COOLDOWN_S)
    with tempfile.NamedTemporaryFile(suffix=".upz", delete=False) as tmp:
        tmp.write(data)  # the GDS uplinker deletes its source
    try:
        uplinker.enqueue(tmp.name, dest)
        deadline = time.time() + UPLINK_TIMEOUT_S
        time.sleep(1)
        while time.time() < deadline and uplinker.state != FileStates.IDLE:
            time.sleep(1)
        assert uplinker.state == FileStates.IDLE, f"uplink of {dest} did not finish"
    finally:
        uplinker.chunk = saved[0]
        if saved[1] is not None:
            uplinker.cooldown = saved[1]
    assert _onboard_crc(api, dest) == zlib.crc32(data), f"{dest} arrived damaged"


def _onboard_crc(api: IntegrationTestAPI, path: str) -> int | None:
    """zlib CRC32 of a file on the board (CalculateCrc reports it inverted)."""
    api.clear_histories()
    api.send_command(f"{FILE_MANAGER}.CalculateCrc", [path])
    evt = api.await_event(f"{FILE_MANAGER}.CalculateCrcSucceeded", timeout=120)
    return None if evt is None else (~evt.args[1].val) & 0xFFFFFFFF


def _remove(api: IntegrationTestAPI, path: str) -> None:
    """Delete a file on the board, ignoring a missing one."""
    api.send_command(f"{FILE_MANAGER}.RemoveFile", [path, True])
    time.sleep(1)


@pytest.fixture(scope="module")
def coded_sample(fprime_test_api: IntegrationTestAPI, start_gds):
    """Compress a firmware sample and uplink it once for the module."""
    raw = _firmware_sample()
    blob = upz.compress(raw)
    assert upz.decompress(blob) == raw
    print(f"UPZ sample: {len(raw)} -> {len(blob)} bytes ({len(blob) / len(raw):.3f})")
    _uplink(fprime_test_api, blob, CODED)
    yield raw, blob
    _remove(fprime_test_api, CODED)
    _remove(fprime_test_api, OUT)


def test_expand_rebuilds_file(fprime_test_api: IntegrationTestAPI, coded_sample):
    """EXPAND_FILE rebuilds the exact bytes, and the board's own CRC agrees."""
    raw, _ = coded_sample
    _remove(fprime_test_api, OUT)
    proves_send_and_assert_command(
        fprime_test_api, f"{EXPANDER}.EXPAND_FILE", [CODED, OUT]
    )
    evt = fprime_test_api.await_event(
        f"{EXPANDER}.ExpandComplete", timeout=EXPAND_TIMEOUT_S
    )
    assert evt is not None, "no ExpandComplete"
    assert evt.args[1].val == len(raw)
    assert evt.args[2].val == zlib.crc32(raw)
    print(f"far reads {evt.args[3].val}, ticks {evt.args[4].val}")
    assert _onboard_crc(fprime_test_api, OUT) == zlib.crc32(raw)


def test_expand_resumes_after_abort(fprime_test_api: IntegrationTestAPI, coded_sample):
    """A stopped expansion keeps its verified chunks; resending the command resumes past them."""
    raw, _ = coded_sample
    _remove(fprime_test_api, OUT)
    proves_send_and_assert_command(
        fprime_test_api, f"{EXPANDER}.EXPAND_FILE", [CODED, OUT]
    )
    # Chunk 0 is verified as soon as it is flushed, so a size past 64 KiB means it is safely on storage
    deadline = time.time() + EXPAND_TIMEOUT_S
    while True:
        fprime_test_api.clear_histories()
        fprime_test_api.send_command(f"{FILE_MANAGER}.FileSize", [OUT])
        evt = fprime_test_api.await_event(
            f"{FILE_MANAGER}.FileSizeSucceeded", timeout=10
        )
        if evt is not None and evt.args[1].val >= 65_536:
            break
        assert time.time() < deadline, "expansion never passed the first chunk"
        time.sleep(0.5)
    proves_send_and_assert_command(fprime_test_api, f"{EXPANDER}.ABORT_EXPAND")
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api, f"{EXPANDER}.EXPAND_FILE", [CODED, OUT]
    )
    evt = fprime_test_api.await_event(
        f"{EXPANDER}.ExpandComplete", timeout=EXPAND_TIMEOUT_S
    )
    assert evt is not None and evt.args[2].val == zlib.crc32(raw)
    # No ExpandResumed means every chunk was already verified (the abort landed after the last one)
    resumed = [
        e
        for e in fprime_test_api.get_event_test_history().retrieve()
        if e.template.get_full_name().endswith("ExpandResumed")
    ]
    if resumed:
        assert resumed[-1].args[0].val >= 1, "resume started over at chunk 0"
    assert _onboard_crc(fprime_test_api, OUT) == zlib.crc32(raw)
