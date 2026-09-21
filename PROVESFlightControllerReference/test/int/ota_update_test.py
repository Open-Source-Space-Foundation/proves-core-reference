"""
ota_update_test.py:

UART-only OTA image-swap tests (Update.updater + FlashWorker + MCUBoot) for a
bare flight-controller board. Requires --ota-image and --ota-build-id.

A test that stops early leaves the board on the image it started on. A passing run
ends on the confirmed --ota-image, which only a reflash can undo (CI does this).
"""

import os
import shutil
import tempfile
import time
import zlib
from datetime import datetime

import pytest
from common import proves_send_and_assert_command, resync_sequence_number
from fprime_gds.common.files.helpers import FileStates
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

pytestmark = [pytest.mark.ota, pytest.mark.uart_only]

UPDATER = "Update.updater"
WORKER = "Update.worker"
FILE_MANAGER = "FileHandling.fileManager"
RESET_MANAGER = "ReferenceDeployment.resetManager"
VERSION = "CdhCore.version"
DEFRAMER = "ComCcsdsUart.tcSecurityDeframer"

UPDATE_TIMEOUT_S = 180
PREPARE_TIMEOUT_S = 60
BOOT_TIMEOUT_SWAP_S = 150
BOOT_TIMEOUT_PLAIN_S = 60
# ~35 min for a ~676KB image at --file-uplink-cooldown 0.6; lower once #457 lands.
UART_UPLINK_TIMEOUT_S = 3600
# Larger chunks overflow the 248B TC frame and every DATA packet is silently dropped.
MAX_UPLINK_CHUNK = 204


def _assert_uplink_chunk(api: IntegrationTestAPI) -> None:
    """Fail fast on an uplink chunk size the board would silently drop."""
    chunk = api.pipeline.files.uplinker.chunk
    assert chunk <= MAX_UPLINK_CHUNK, (
        f"file-uplink chunk size {chunk} > {MAX_UPLINK_CHUNK}: "
        f"pass --file-uplink-chunk-size {MAX_UPLINK_CHUNK} to pytest"
    )


def _local_crc(path: str) -> int:
    """CRC32 matching FileHandling.fileManager.CalculateCrc (zlib.crc32 ^ 0xFFFFFFFF)."""
    crc = 0
    with open(path, "rb") as fh:
        while chunk := fh.read(8192):
            crc = zlib.crc32(chunk, crc)
    return ~crc & 0xFFFFFFFF


def _onboard_crc(
    api: IntegrationTestAPI, dest: str, timeout: float = 120
) -> int | None:
    """Return the board-computed CRC32 of ``dest``, or None on failure."""
    api.send_command(f"{FILE_MANAGER}.CalculateCrc", [dest])
    evt = api.await_event(f"{FILE_MANAGER}.CalculateCrcSucceeded", timeout=timeout)
    if evt is None:
        return None
    return evt.args[1].val


def _uplink_uart(
    api: IntegrationTestAPI, local: str, dest: str, timeout: float
) -> bool:
    """Uplink a temp copy of ``local`` (the GDS uplinker deletes its source) and wait for IDLE."""
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
        staged = tmp.name
    shutil.copyfile(local, staged)
    uplinker = api.pipeline.files.uplinker
    uplinker.enqueue(staged, dest)
    deadline = time.time() + timeout
    time.sleep(1)
    while time.time() < deadline and uplinker.state != FileStates.IDLE:
        time.sleep(1)
    return uplinker.state == FileStates.IDLE


def _uplink_and_verify_crc(api: IntegrationTestAPI, local: str, dest: str) -> int:
    """Uplink ``local`` to ``dest``, assert the on-board CRC matches, and return it."""
    expected = _local_crc(local)
    idle = _uplink_uart(api, local, dest, UART_UPLINK_TIMEOUT_S)
    assert idle, f"UART uplinker did not return to IDLE for {dest}"
    actual = _onboard_crc(api, dest)
    actual_str = "None" if actual is None else f"0x{actual:08x}"
    assert actual == expected, (
        f"on-board CRC {actual_str} != expected 0x{expected:08x} for {dest}"
    )
    return expected


def _resync(api: IntegrationTestAPI) -> None:
    """Resync the auth sequence number. Must be the last command before a COLD_RESET."""
    resync_sequence_number(api, DEFRAMER)


def _apply_uart_only_repeater(api: IntegrationTestAPI) -> None:
    """Downlink over UART only. The parameter is RAM-only, so re-apply after every reboot."""
    # [uart, lora, sband]; array arguments are passed as a JSON string.
    api.send_command(
        "ReferenceDeployment.downlinkRepeater.CHANNEL_ENABLED_PRM_SET",
        ['["ENABLED", "DISABLED", "DISABLED"]'],
    )
    time.sleep(1)


def _cold_reset(api: IntegrationTestAPI) -> TimeType:
    """Send COLD_RESET (it never completes; the board reboots) and return the send time."""
    start = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )
    api.send_command(f"{RESET_MANAGER}.COLD_RESET")
    return start


def _wait_for_boot(api: IntegrationTestAPI, start: TimeType, timeout: float) -> None:
    """Wait for the boot-time FrameworkVersion event, retrying COLD_RESET once if dropped."""
    evt = api.await_event(f"{VERSION}.FrameworkVersion", start=start, timeout=timeout)
    if evt is not None:
        return
    _resync(api)
    start = _cold_reset(api)
    evt = api.await_event(f"{VERSION}.FrameworkVersion", start=start, timeout=timeout)
    assert evt is not None, (
        f"no CdhCore.version.FrameworkVersion after COLD_RESET within "
        f"{timeout}s (including one retry)"
    )


def _project_version_after_boot(
    api: IntegrationTestAPI, start: TimeType, timeout: float
) -> str:
    """Wait for the reboot, resync, and return the project version string."""
    _wait_for_boot(api, start, timeout)
    _resync(api)
    evt = api.await_event(f"{VERSION}.ProjectVersion", timeout=5)
    return str(evt.args[0].val) if evt is not None else _current_version(api)


def _current_version(api: IntegrationTestAPI) -> str:
    """Ask the board for its project version string."""
    for _ in range(3):
        api.clear_histories()
        api.send_command(f"{VERSION}.VERSION", ["PROJECT"])
        evt = api.await_event(f"{VERSION}.ProjectVersion", timeout=10)
        if evt is not None:
            return str(evt.args[0].val)
        _resync(api)
    raise AssertionError("no ProjectVersion event from the board")


def _reboot_and_get_version(api: IntegrationTestAPI, swap_expected: bool) -> str:
    """COLD_RESET and return the booted project version, leaving the link resynced."""
    timeout = BOOT_TIMEOUT_SWAP_S if swap_expected else BOOT_TIMEOUT_PLAIN_S
    start = _cold_reset(api)
    version = _project_version_after_boot(api, start, timeout)
    _apply_uart_only_repeater(api)
    _resync(api)
    return version


def _configure_next_boot(api: IntegrationTestAPI, mode: str) -> None:
    """Arm the image in slot1 for the next boot and resync, ready for a COLD_RESET."""
    proves_send_and_assert_command(api, f"{UPDATER}.CONFIGURE_NEXT_BOOT", args=[mode])
    api.assert_event(f"{UPDATER}.SetNextBoot", timeout=10)
    _resync(api)


@pytest.fixture
def restore_original_image(fprime_test_api: IntegrationTestAPI, start_gds):
    """Leave the board on the image it started on if the test stops before CONFIRM_UPDATE.

    A confirmed image cannot be swapped back over the link; reflash to undo it.
    """
    api = fprime_test_api
    _resync(api)
    original = _current_version(api)
    outcome = {"confirmed": False}
    yield original, outcome
    if outcome["confirmed"]:
        return
    _resync(api)
    if _current_version(api) != original:
        # Unconfirmed, so MCUBoot reverts on a plain reboot.
        restored = _reboot_and_get_version(api, swap_expected=False)
        assert restored == original, (
            f"board is running {restored!r}, not the original {original!r}"
        )
    # Erasing slot1 cancels a swap that is still pending.
    proves_send_and_assert_command(api, f"{UPDATER}.PREPARE_UPDATE")
    api.assert_event(f"{UPDATER}.PrepareUpdateSucceeded", timeout=PREPARE_TIMEOUT_S)


def test_ota_negative_paths(
    fprime_test_api: IntegrationTestAPI,
    start_gds,
    ota_config,
):
    """A wrong CRC is rejected before any flash write, and an update needs a fresh PREPARE."""
    _assert_uplink_chunk(fprime_test_api)

    _apply_uart_only_repeater(fprime_test_api)
    api = fprime_test_api
    # Unique file name per run so a stale file on the SD card is never reused.
    dest = f"/otaneg{os.getpid() % 100000}.bin"

    junk_local = None
    try:
        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
            tmp.write(os.urandom(1024))
            junk_local = tmp.name

        idle = _uplink_uart(api, junk_local, dest, 60)
        assert idle, f"UART uplinker did not return to IDLE for {dest}"
        good_crc = _onboard_crc(api, dest, timeout=30)
        assert good_crc is not None, "could not read on-board CRC of uploaded image"
        wrong_crc = (good_crc ^ 0xFFFFFFFF) & 0xFFFFFFFF

        api.clear_histories()
        proves_send_and_assert_command(api, f"{UPDATER}.PREPARE_UPDATE")
        api.assert_event(f"{UPDATER}.PrepareUpdateSucceeded", timeout=PREPARE_TIMEOUT_S)

        api.clear_histories()
        api.send_command(f"{UPDATER}.UPDATE_IMAGE_FROM", [dest, str(wrong_crc)])
        evt = api.await_event(
            f"{WORKER}.ImageFileCrcMismatch", timeout=UPDATE_TIMEOUT_S
        )
        assert evt is not None, "expected ImageFileCrcMismatch on wrong-CRC update"
        # The updater is BUSY until it reports the failure.
        api.assert_event(f"{UPDATER}.UpdateFailed", timeout=30)

        # The failed attempt consumed the PREPARE, so even the right CRC is refused.
        api.clear_histories()
        api.send_command(f"{UPDATER}.UPDATE_IMAGE_FROM", [dest, str(good_crc)])
        evt = api.await_event(f"{WORKER}.NoImagePrepared", timeout=30)
        assert evt is not None, (
            "expected NoImagePrepared for UPDATE_IMAGE_FROM without PREPARE_UPDATE"
        )
    finally:
        try:
            api.clear_histories()
            api.send_command(f"{FILE_MANAGER}.RemoveFile", [dest, "false"])
            ok = api.await_event(f"{FILE_MANAGER}.RemoveFileSucceeded", timeout=10)
            if ok is None:
                err = api.await_event(f"{FILE_MANAGER}.FileRemoveError", timeout=1)
                print(
                    f"cleanup: failed to remove {dest!r} from board "
                    f"({err.args if err is not None else 'no RemoveFile event seen'})"
                )
        except Exception as cleanup_exc:
            print(
                f"cleanup: exception while removing {dest!r} from board: {cleanup_exc!r}"
            )
        if junk_local is not None:
            try:
                os.remove(junk_local)
            except OSError:
                pass


def test_ota_invalid_image_is_not_booted(
    fprime_test_api: IntegrationTestAPI,
    start_gds,
    ota_config,
    restore_original_image,
):
    """A file that is not a signed image passes the CRC gate, but MCUBoot refuses to boot it."""
    original, _ = restore_original_image
    _assert_uplink_chunk(fprime_test_api)

    _apply_uart_only_repeater(fprime_test_api)
    api = fprime_test_api
    # Unique file name per run so a stale file on the SD card is never reused.
    dest = f"/otabad{os.getpid() % 100000}.bin"

    junk_local = None
    try:
        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as tmp:
            tmp.write(os.urandom(1024))
            junk_local = tmp.name
        crc = _uplink_and_verify_crc(api, junk_local, dest)

        api.clear_histories()
        proves_send_and_assert_command(api, f"{UPDATER}.PREPARE_UPDATE")
        api.assert_event(f"{UPDATER}.PrepareUpdateSucceeded", timeout=PREPARE_TIMEOUT_S)

        # The operator's CRC is correct, so the junk is written to slot1 and armed.
        api.clear_histories()
        proves_send_and_assert_command(
            api, f"{UPDATER}.UPDATE_IMAGE_FROM", args=[dest, str(crc)]
        )
        api.assert_event(f"{UPDATER}.UpdateSucceeded", timeout=UPDATE_TIMEOUT_S)

        _configure_next_boot(api, "TEST")

        version = _reboot_and_get_version(api, swap_expected=True)
        assert version == original, (
            f"board is running {version!r}, not the original {original!r}, "
            "after arming an invalid image"
        )
    finally:
        try:
            api.clear_histories()
            api.send_command(f"{FILE_MANAGER}.RemoveFile", [dest, "false"])
            ok = api.await_event(f"{FILE_MANAGER}.RemoveFileSucceeded", timeout=10)
            if ok is None:
                err = api.await_event(f"{FILE_MANAGER}.FileRemoveError", timeout=1)
                print(
                    f"cleanup: failed to remove {dest!r} from board "
                    f"({err.args if err is not None else 'no RemoveFile event seen'})"
                )
        except Exception as cleanup_exc:
            print(
                f"cleanup: exception while removing {dest!r} from board: {cleanup_exc!r}"
            )
        if junk_local is not None:
            try:
                os.remove(junk_local)
            except OSError:
                pass


def test_ota_swap_and_revert(
    fprime_test_api: IntegrationTestAPI,
    start_gds,
    ota_config,
    restore_original_image,
):
    """Uplink -> TEST swap boots new image -> unconfirmed reboot reverts -> re-swap -> confirm persists."""
    image, build_id = ota_config
    original, outcome = restore_original_image
    assert build_id not in original, (
        f"board already runs {original!r}; flash the original image first"
    )
    _assert_uplink_chunk(fprime_test_api)

    _apply_uart_only_repeater(fprime_test_api)
    api = fprime_test_api
    # Unique file name per run so a stale file on the SD card is never reused.
    dest = f"/ota-{os.getpid() % 100000}.bin"

    try:
        crc = _uplink_and_verify_crc(api, image, dest)

        api.clear_histories()
        proves_send_and_assert_command(api, f"{UPDATER}.PREPARE_UPDATE")
        api.assert_event(f"{UPDATER}.PrepareUpdateSucceeded", timeout=PREPARE_TIMEOUT_S)

        api.clear_histories()
        proves_send_and_assert_command(
            api, f"{UPDATER}.UPDATE_IMAGE_FROM", args=[dest, str(crc)]
        )
        api.assert_event(f"{UPDATER}.UpdateSucceeded", timeout=UPDATE_TIMEOUT_S)

        _configure_next_boot(api, "TEST")

        version = _reboot_and_get_version(api, swap_expected=True)
        assert build_id in version, (
            f"booted project version {version!r} does not contain build id "
            f"{build_id!r} — TEST image did not take"
        )

        # Not confirmed, so MCUBoot must revert on the next reboot.
        reverted = _reboot_and_get_version(api, swap_expected=False)
        assert build_id not in reverted, (
            f"project version {reverted!r} still contains build id {build_id!r} "
            f"after second reboot — MCUBoot did not auto-revert"
        )

        # The revert swapped the new image back into slot1; re-arm it and confirm.
        _configure_next_boot(api, "TEST")

        version = _reboot_and_get_version(api, swap_expected=True)
        assert build_id in version, (
            f"booted project version {version!r} does not contain build id "
            f"{build_id!r} — re-staged TEST image did not take"
        )

        proves_send_and_assert_command(api, f"{UPDATER}.CONFIRM_UPDATE")
        api.assert_event(f"{UPDATER}.ConfirmBoot", timeout=10)
        outcome["confirmed"] = True
        _resync(api)

        confirmed = _reboot_and_get_version(api, swap_expected=False)
        assert build_id in confirmed, (
            f"project version {confirmed!r} does not contain build id "
            f"{build_id!r} after CONFIRM_UPDATE + reboot — confirmed image "
            "did not persist"
        )
    finally:
        try:
            api.clear_histories()
            api.send_command(f"{FILE_MANAGER}.RemoveFile", [dest, "false"])
            ok = api.await_event(f"{FILE_MANAGER}.RemoveFileSucceeded", timeout=10)
            if ok is None:
                err = api.await_event(f"{FILE_MANAGER}.FileRemoveError", timeout=1)
                print(
                    f"cleanup: failed to remove {dest!r} from board "
                    f"({err.args if err is not None else 'no RemoveFile event seen'})"
                )
        except Exception as cleanup_exc:
            print(
                f"cleanup: exception while removing {dest!r} from board: {cleanup_exc!r}"
            )
