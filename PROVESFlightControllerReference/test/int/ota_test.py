"""
ota_test.py:

Integration tests for the OTA update path (Update.updater command surface and
the Update.worker / FlashWorker implementation behind it).

These tests exercise the ground-commandable portion of the OTA flow that does
not require a reboot: PREPARE_UPDATE, UPDATE_IMAGE_FROM, and the failure modes
of the image write. They intentionally never send CONFIGURE_NEXT_BOOT, so the
board never arms an MCUboot upgrade and the running image is unaffected. The
test image written to the update slot is pseudo-random data that MCUboot would
refuse to boot even if it were armed.

Tests are ordered (test_01 .. test_06) because the FlashWorker state machine
(IDLE -> PREPARE -> UPDATE) persists across tests within a boot.
"""

import random
import tempfile
import zlib
from pathlib import Path

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

updater = "Update.updater"
worker = "Update.worker"
fileManager = "FileHandling.fileManager"

# A 1 MiB slot erase blocks the worker for a while and the multi-KB file
# uplink is impractically slow and flaky over the half-duplex LoRa link, so
# run the OTA suite on the UART pass only.
pytestmark = pytest.mark.uart_only(
    reason="slot erase and multi-KB file uplink are too slow for the LoRa radio pass"
)

# Deterministic pseudo-random test image: same bytes (and therefore the same
# CRC) on every run. 8 KiB spans multiple CONFIG_IMG_BLOCK_BUF_SIZE (512 B)
# chunks so the buffered flash write loop is exercised.
TEST_IMAGE_BYTES = random.Random(429).randbytes(8 * 1024)
TEST_IMAGE_DEST = "/update/ota_test_image.bin"
MISSING_FILE = "/update/does_not_exist.bin"

# Worst-case erase of the 1 MiB update slot on the external QSPI flash.
PREPARE_TIMEOUT_S = 90
# CRC + chunked write of the 8 KiB test image (5 ms inter-chunk delay).
UPDATE_TIMEOUT_S = 60


def ota_crc32(data: bytes) -> int:
    """CRC32 in the convention shared by tools/bin/calculate-crc.py and
    Os::File::calculateCrc on the flight side: zlib's CRC32 without the
    final XOR (hence the inversion of zlib's already-inverted result)."""
    return ~zlib.crc32(data) & 0xFFFFFFFF


def send_update_and_await_failure(
    fprime_test_api: IntegrationTestAPI,
    file_path: str,
    crc32: int,
    timeout: int = UPDATE_TIMEOUT_S,
):
    """Send UPDATE_IMAGE_FROM and return the UpdateFailed event's status arg."""
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(
        f"{updater}.UPDATE_IMAGE_FROM", [file_path, str(crc32)]
    )
    event = fprime_test_api.assert_event(f"{updater}.UpdateFailed", timeout=timeout)
    return str(event.args[0].val)


def prepare_and_await_success(fprime_test_api: IntegrationTestAPI):
    """Send PREPARE_UPDATE and wait for the (slow) slot erase to finish."""
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(f"{updater}.PREPARE_UPDATE")
    fprime_test_api.assert_event(
        f"{updater}.PrepareUpdateSucceeded", timeout=PREPARE_TIMEOUT_S
    )


def test_01_update_without_prepare_fails(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """UPDATE_IMAGE_FROM without a prior PREPARE_UPDATE must be refused.

    The first attempt normalizes the worker state machine: whatever state a
    previous run left behind, a failed update returns it to IDLE. The second
    attempt then deterministically exercises the unprepared guard.
    """
    send_update_and_await_failure(fprime_test_api, MISSING_FILE, 0)

    status = send_update_and_await_failure(fprime_test_api, MISSING_FILE, 0)
    assert status == "UNPREPARED"
    fprime_test_api.assert_event(f"{worker}.NoImagePrepared", timeout=5)


def test_02_prepare_update_succeeds(fprime_test_api: IntegrationTestAPI, start_gds):
    """PREPARE_UPDATE erases the update slot and reports success."""
    prepare_and_await_success(fprime_test_api)


def test_03_update_from_missing_file_fails(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """A prepared update from a nonexistent file reports IMAGE_FILE_READ_ERROR."""
    status = send_update_and_await_failure(fprime_test_api, MISSING_FILE, 0)
    assert status == "IMAGE_FILE_READ_ERROR"
    fprime_test_api.assert_event(f"{worker}.ImageFileReadError", timeout=5)


def test_04_update_with_bad_crc_is_rejected(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """An uplinked image whose CRC does not match the commanded CRC must be
    rejected with IMAGE_CRC_MISMATCH and must never be written to flash."""
    prepare_and_await_success(fprime_test_api)

    fprime_test_api.send_command(f"{fileManager}.CreateDirectory", ["/update"])
    fprime_test_api.send_command(f"{fileManager}.RemoveFile", [TEST_IMAGE_DEST, True])
    with tempfile.TemporaryDirectory() as tempdir:
        image_path = Path(tempdir) / "ota_test_image.bin"
        image_path.write_bytes(TEST_IMAGE_BYTES)
        fprime_test_api.uplink_file_and_await_completion(
            str(image_path), TEST_IMAGE_DEST, timeout=120
        )

    bad_crc = ota_crc32(TEST_IMAGE_BYTES) ^ 0xDEADBEEF
    status = send_update_and_await_failure(fprime_test_api, TEST_IMAGE_DEST, bad_crc)
    assert status == "IMAGE_CRC_MISMATCH"
    fprime_test_api.assert_event(f"{worker}.ImageFileCrcMismatch", timeout=5)


def test_05_update_retry_with_correct_crc_succeeds(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """After a CRC rejection the flash slot is untouched, so a retry with the
    correct CRC succeeds without another PREPARE_UPDATE. This also verifies
    end-to-end that the ground CRC convention (tools/bin/calculate-crc.py)
    matches the flight-side Os::File::calculateCrc."""
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(
        f"{updater}.UPDATE_IMAGE_FROM",
        [TEST_IMAGE_DEST, str(ota_crc32(TEST_IMAGE_BYTES))],
    )
    fprime_test_api.assert_event(f"{updater}.UpdateSucceeded", timeout=UPDATE_TIMEOUT_S)


def test_06_second_update_requires_new_prepare(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """A completed update consumes the prepared slot: another UPDATE_IMAGE_FROM
    must be refused until PREPARE_UPDATE is run again."""
    status = send_update_and_await_failure(
        fprime_test_api, TEST_IMAGE_DEST, ota_crc32(TEST_IMAGE_BYTES)
    )
    assert status == "UNPREPARED"
    fprime_test_api.assert_event(f"{worker}.NoImagePrepared", timeout=5)
