"""
ota_update_test.py:

Integration tests for the over-the-air (OTA) flight-software image swap path
(Update.updater + Components.FlashWorker, backed by MCUBoot).

These tests exercise the full "swap and revert" lifecycle end to end against real
hardware:

  1. Uplink a signed image to a unique destination on the on-board filesystem.
  2. Verify the uplink with the on-board CalculateCrc oracle.
  3. PREPARE_UPDATE -> UPDATE_IMAGE_FROM -> CONFIGURE_NEXT_BOOT[TEST].
  4. COLD_RESET and assert the newly-booted image reports the --ota-build-id.
  5. Deliberately skip CONFIRM_UPDATE, COLD_RESET again, and assert MCUBoot has
     auto-reverted to the previous (confirmed) image.

They are parametrized over the uplink transport:
  * ``uart``  — fast, reliable link. The image is enqueued on the GDS uplinker
                and we poll for IDLE. Runs on a UART bench (skipped with --with-radio).
  * ``lora``  — lossy half-duplex radio. The image is re-uplinked to the same
                destination until the on-board CRC matches (idempotent offset
                writes). Marked ``slow`` and only runs with --with-radio.

Because every variant issues COLD_RESET (which severs the RF link mid-run) each
is also tagged ``uart_only`` so the suite's collection logic keeps them off the
pure-radio regression path.

Requires --ota-image=<path to zephyr.signed.bin> and --ota-build-id=<marker>;
without them the ``ota_config`` fixture skips the whole module. See issue for
context: OTA image-swap integration coverage.
"""

import os
import shutil
import tempfile
import time
import zlib
from datetime import datetime

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.files.helpers import FileStates
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# Every variant COLD_RESETs the board, severing the RF link, so this module is
# uart_only in the same sense as reset_manager_test / radio_test.
pytestmark = [pytest.mark.ota, pytest.mark.uart_only]

UPDATER = "Update.updater"
WORKER = "Update.worker"
FILE_MANAGER = "FileHandling.fileManager"
RESET_MANAGER = "ReferenceDeployment.resetManager"
VERSION = "CdhCore.version"

# Flash write of a full signed image is the slowest step in the sequence; give it
# a generous ceiling so a legitimately-long erase/write does not time out.
UPDATE_TIMEOUT_S = 180
PREPARE_TIMEOUT_S = 60
BOOT_TIMEOUT_S = 30

# UART uplink of a multi-hundred-KB image; the LoRa path uses its own longer budget.
UART_UPLINK_TIMEOUT_S = 900
# LoRa is airtime-bound and lossy: a full image can take many minutes and may need
# re-uplinking a few times before the on-board CRC matches.
LORA_UPLINK_TIMEOUT_S = 3600
LORA_UPLINK_ATTEMPTS = 4


def _local_crc(path: str) -> int:
    """CRC32 matching FileHandling.fileManager.CalculateCrc (zlib.crc32 ^ 0xFFFFFFFF)."""
    crc = 0
    with open(path, "rb") as fh:
        while chunk := fh.read(8192):
            crc = zlib.crc32(chunk, crc)
    return ~crc & 0xFFFFFFFF


def _onboard_crc(api: IntegrationTestAPI, dest: str, timeout: float = 30) -> int | None:
    """Return the board-computed CRC32 of ``dest`` via CalculateCrc, or None on failure."""
    api.send_command(f"{FILE_MANAGER}.CalculateCrc", [dest])
    evt = api.await_event(f"{FILE_MANAGER}.CalculateCrcSucceeded", timeout=timeout)
    if evt is None:
        return None
    # CalculateCrcSucceeded args: (file_name, crc) — the CRC is arg[1].
    return evt.args[1].val


def _uplink_uart(
    api: IntegrationTestAPI, local: str, dest: str, timeout: float
) -> bool:
    """Enqueue ``local`` for uplink to ``dest`` and poll the uplinker to IDLE.

    The GDS uplinker deletes its source file once the transfer completes (it
    expects a staging copy), so enqueue a sacrificial temp copy of ``local``.
    """
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


def _uplink_lora_until_crc(
    api: IntegrationTestAPI,
    local: str,
    dest: str,
    expected_crc: int,
    timeout: float,
    attempts: int,
) -> bool:
    """Re-uplink ``local`` to the same ``dest`` until the on-board CRC matches.

    Offset writes are idempotent (no truncate), so re-sending the whole file over
    a lossy radio link repairs any DATA frames dropped over the air. Returns True
    once CalculateCrc reports a match, False if all attempts are exhausted.
    """
    for attempt in range(attempts):
        # Half-duplex: the flight's own TM transmissions blind its receiver, so
        # uplink into a silent board (commanding works with TX disabled) and only
        # re-enable the downlink to read the CRC back.
        api.send_command("ReferenceDeployment.lora.TRANSMIT", ["DISABLED"])
        time.sleep(5)
        _uplink_uart(api, local, dest, timeout)
        api.send_command("ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET", [20])
        time.sleep(2)
        api.send_command("ReferenceDeployment.lora.TRANSMIT", ["ENABLED"])
        time.sleep(10)
        # A 700KB on-board CRC plus a divider-paced radio downlink far exceeds
        # the UART-tuned default timeout; retry the command itself as well since
        # a single command frame can be lost over the air.
        actual = None
        for _ in range(3):
            actual = _onboard_crc(api, dest, timeout=240)
            if actual is not None:
                break
        if actual == expected_crc:
            return True
        print(
            f"[ota] lora uplink attempt {attempt + 1}/{attempts}: "
            f"crc mismatch (expected 0x{expected_crc:08x}, got "
            f"{'None' if actual is None else f'0x{actual:08x}'}) — re-uplinking"
        )
    return False


def _uplink_and_verify_crc(
    api: IntegrationTestAPI, transport: str, local: str, dest: str
) -> int:
    """Uplink ``local`` to ``dest`` for the given transport and assert on-board CRC.

    Returns the verified CRC32 (== local CRC) for use with UPDATE_IMAGE_FROM.
    """
    expected = _local_crc(local)
    if transport == "lora":
        ok = _uplink_lora_until_crc(
            api, local, dest, expected, LORA_UPLINK_TIMEOUT_S, LORA_UPLINK_ATTEMPTS
        )
        assert ok, f"LoRa uplink of {local} never matched CRC 0x{expected:08x}"
    else:
        idle = _uplink_uart(api, local, dest, UART_UPLINK_TIMEOUT_S)
        assert idle, f"UART uplinker did not return to IDLE for {dest}"
        actual = _onboard_crc(api, dest)
        actual_str = "None" if actual is None else f"0x{actual:08x}"
        assert actual == expected, (
            f"on-board CRC {actual_str} != expected 0x{expected:08x} for {dest}"
        )
    return expected


def _resync_sequence_number(
    api: IntegrationTestAPI, request: pytest.FixtureRequest
) -> None:
    """Re-read the flight sequence number and rewrite the GDS framing file.

    Mirrors sync_sequence_number_test: after a reboot the flight-side counter has
    advanced past whatever the GDS framing plugin last persisted, so the next few
    uplink commands would be rejected as replays until we resync.
    """
    link = request.config.getoption("--sync-deframer", default=None)
    if link is None:
        link = (
            "lora"
            if request.config.getoption("--with-radio", default=False)
            else "uart"
        )
    deframer = {
        "uart": "ComCcsdsUart.tcSecurityDeframer",
        "lora": "ComCcsdsLora.tcSecurityDeframer",
    }[link]
    proves_send_and_assert_command(api, f"{deframer}.GET_SEQ_NUM")
    evt = api.assert_event(f"{deframer}.SequenceNumberGet", timeout=5)
    seq_num = evt.args[0].val
    with open("./Framing/src/sequence_number.bin", "w", encoding="utf-8") as f:
        f.write(str(seq_num))


def _reenable_radio_after_boot(api: IntegrationTestAPI, transport: str) -> None:
    """Blindly re-enable the flight LoRa downlink after a reboot (lora only).

    TRANSMIT resets to DISABLED on every flight reset, so post-reboot boot events
    never downlink over the radio until we re-enable. The uplink direction works
    with TX disabled, so these fire-and-forget sends get through; the divider must
    be set while TX is still DISABLED (parameters latch on enable).
    """
    if transport != "lora":
        return
    time.sleep(10)  # let the board finish booting before commanding
    for _ in range(3):
        api.send_command("ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET", [20])
        time.sleep(2)
        api.send_command("ReferenceDeployment.lora.TRANSMIT", ["ENABLED"])
        time.sleep(8)


def _configure_repeater_for_uart(api: IntegrationTestAPI, transport: str) -> None:
    """Route file downlink to the UART channel only (uart transport).

    downlinkRepeater.CHANNEL_ENABLED is [uart, lora, sband] and RAM-only: it
    resets on every reboot, and this test reboots the board twice. If the LoRa
    channel is left enabled during a UART run, file downlink buffers queue on
    the (slow or dead) radio path and the transfer throttles to the slowest
    consumer -- a few-minute uplink becomes tens of minutes and reads as a rig
    failure. Re-apply after every reboot, not just at test start.

    lora transport is left untouched: its channel setup is handled by
    _reenable_radio_after_boot and the board's saved parameters.
    """
    if transport != "uart":
        return
    # The GDS command layer takes array arguments as a JSON string, not a list.
    api.send_command(
        "ReferenceDeployment.downlinkRepeater.CHANNEL_ENABLED_PRM_SET",
        ['["ENABLED", "DISABLED", "DISABLED"]'],
    )
    time.sleep(1)


def _cold_reset(api: IntegrationTestAPI) -> TimeType:
    """Issue COLD_RESET without expecting an OK response and return the send time.

    The board reboots before the command can complete, so the command dispatcher
    reports EXECUTION_ERROR — exactly as reset_manager_test relies on. We assert
    the restart by looking for the boot-time version events instead.
    """
    start = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )
    api.send_command(f"{RESET_MANAGER}.COLD_RESET")
    return start


def _project_version_after_boot(
    api: IntegrationTestAPI,
    start: TimeType,
    request: pytest.FixtureRequest,
    timeout: float = BOOT_TIMEOUT_S,
    transport: str = "uart",
) -> str:
    """Wait for the boot to complete and return the reported project version string.

    The Version component emits FrameworkVersion/ProjectVersion at startup (see
    reset_manager_test, which keys restart detection off FrameworkVersion). We wait
    for FrameworkVersion to confirm the reboot, then read ProjectVersion — falling
    back to an explicit VERSION[PROJECT] command if the boot-time event was missed.
    The fallback must resync the auth sequence number first: commands sent before
    resyncing after a reboot are silently rejected by the security deframer.
    """
    if transport == "lora":
        # Boot-time events are emitted while LoRa TX is still DISABLED (the
        # default after every reset) and are lost over the air; go straight to
        # the commanded fallback below.
        evt = None
    else:
        api.assert_event(f"{VERSION}.FrameworkVersion", start=start, timeout=timeout)
        evt = api.await_event(f"{VERSION}.ProjectVersion", timeout=5)
    if evt is None:
        _resync_sequence_number(api, request)
        for _ in range(3):
            api.send_command(f"{VERSION}.VERSION", ["PROJECT"])
            evt = api.await_event(f"{VERSION}.ProjectVersion", timeout=10)
            if evt is not None:
                break
            _resync_sequence_number(api, request)
    assert evt is not None, "no ProjectVersion event after reboot"
    return str(evt.args[0].val)


@pytest.mark.parametrize(
    "transport",
    [
        pytest.param("uart", id="uart"),
        pytest.param("lora", id="lora", marks=pytest.mark.slow),
    ],
)
def test_ota_swap_and_revert(
    fprime_test_api: IntegrationTestAPI,
    start_gds,
    request: pytest.FixtureRequest,
    ota_config,
    transport: str,
):
    """OTA image swap in TEST mode boots the new image, then auto-reverts.

    Uplinks --ota-image, verifies it on board, stages it as the TEST next-boot,
    reboots and asserts the new build-id is running, then (without CONFIRM_UPDATE)
    reboots again and asserts MCUBoot has reverted to the previous image.
    """
    with_radio = request.config.getoption("--with-radio", default=False)
    if transport == "lora" and not with_radio:
        pytest.skip("lora transport requires --with-radio")
    if transport == "uart" and with_radio:
        pytest.skip(
            "uart transport is skipped on the radio path (use --with-radio for lora)"
        )

    image, build_id = ota_config
    api = fprime_test_api
    # Unique 8.3-friendly destination (GRC FATFS is 8.3 only); include the pid so
    # reruns do not collide with a stale file.
    dest = f"/ota{os.getpid() % 100000}.bin"

    try:
        # (a)/(b) Uplink the signed image and verify the on-board CRC.
        _configure_repeater_for_uart(api, transport)
        crc = _uplink_and_verify_crc(api, transport, image, dest)

        # (c) Stage the update: prepare -> write image -> configure TEST next boot.
        api.clear_histories()
        proves_send_and_assert_command(api, f"{UPDATER}.PREPARE_UPDATE")
        api.assert_event(f"{UPDATER}.PrepareUpdateSucceeded", timeout=PREPARE_TIMEOUT_S)

        api.clear_histories()
        proves_send_and_assert_command(
            api, f"{UPDATER}.UPDATE_IMAGE_FROM", args=[dest, str(crc)]
        )
        api.assert_event(f"{UPDATER}.UpdateSucceeded", timeout=UPDATE_TIMEOUT_S)

        proves_send_and_assert_command(
            api, f"{UPDATER}.CONFIGURE_NEXT_BOOT", args=["TEST"]
        )
        api.assert_event(f"{UPDATER}.SetNextBoot", timeout=10)

        # (d) Reboot into the TEST image and assert the new build is running.
        start = _cold_reset(api)
        _reenable_radio_after_boot(api, transport)
        version = _project_version_after_boot(api, start, request, transport=transport)
        _resync_sequence_number(api, request)
        # Repeater state is RAM-only and just got wiped by the reboot; commands
        # are seq-gated, so re-apply only after the sequence resync above -- and
        # resync AGAIN afterwards: an extra authenticated send between a resync
        # and the next COLD_RESET desyncs the framing plugin's persisted counter
        # (observed as SequenceNumberInvalid rejecting the reset, 2/2 runs).
        _configure_repeater_for_uart(api, transport)
        _resync_sequence_number(api, request)
        assert build_id in version, (
            f"booted project version {version!r} does not contain build id "
            f"{build_id!r} — TEST image did not take"
        )

        # (e) Do NOT CONFIRM_UPDATE. Reboot again; MCUBoot must auto-revert.
        start = _cold_reset(api)
        _reenable_radio_after_boot(api, transport)
        reverted = _project_version_after_boot(api, start, request, transport=transport)
        _resync_sequence_number(api, request)
        _configure_repeater_for_uart(api, transport)
        _resync_sequence_number(api, request)
        assert build_id not in reverted, (
            f"project version {reverted!r} still contains build id {build_id!r} "
            f"after second reboot — MCUBoot did not auto-revert"
        )
    finally:
        # (f) Teardown backstop: force a revert if we bailed mid-TEST, and delete
        # the uploaded image. Best-effort — never mask the real failure.
        try:
            api.send_command(f"{RESET_MANAGER}.COLD_RESET")
            api.await_event(f"{VERSION}.FrameworkVersion", timeout=BOOT_TIMEOUT_S)
            _resync_sequence_number(api, request)
        except Exception:
            pass
        try:
            api.send_command(f"{FILE_MANAGER}.RemoveFile", [dest, "true"])
        except Exception:
            pass


def test_ota_negative_paths(
    fprime_test_api: IntegrationTestAPI,
    start_gds,
    ota_config,
):
    """Failure-mode coverage that does NOT reboot the board.

    * UPDATE_IMAGE_FROM without a prior PREPARE_UPDATE -> FlashWorker.NoImagePrepared
      (the no-prepare gate lives in the worker's updateImage handler, not in
      CONFIGURE_NEXT_BOOT, which is an unconditional boot_request_upgrade).
    * UPDATE_IMAGE_FROM with a wrong CRC -> FlashWorker.ImageFileCrcMismatch.

    Both worker faults surface as (warning) events; the async commands themselves
    dispatch successfully, so we assert on the events rather than a command error.
    The no-prepare case must run FIRST, before this test issues any PREPARE_UPDATE.
    """
    image, _build_id = ota_config
    _configure_repeater_for_uart(fprime_test_api, "uart")
    api = fprime_test_api
    dest = f"/otaneg{os.getpid() % 100000}.bin"

    try:
        # Upload a good image so UPDATE_IMAGE_FROM has a real file to point at.
        idle = _uplink_uart(api, image, dest, UART_UPLINK_TIMEOUT_S)
        assert idle, f"UART uplinker did not return to IDLE for {dest}"
        good_crc = _onboard_crc(api, dest)
        assert good_crc is not None, "could not read on-board CRC of uploaded image"
        wrong_crc = (good_crc ^ 0xFFFFFFFF) & 0xFFFFFFFF

        # UPDATE_IMAGE_FROM with no prior PREPARE_UPDATE -> NoImagePrepared.
        api.clear_histories()
        api.send_command(f"{UPDATER}.UPDATE_IMAGE_FROM", [dest, str(good_crc)])
        evt = api.await_event(f"{WORKER}.NoImagePrepared", timeout=30)
        assert evt is not None, (
            "expected NoImagePrepared for UPDATE_IMAGE_FROM without PREPARE_UPDATE"
        )

        api.clear_histories()
        proves_send_and_assert_command(api, f"{UPDATER}.PREPARE_UPDATE")
        api.assert_event(f"{UPDATER}.PrepareUpdateSucceeded", timeout=PREPARE_TIMEOUT_S)

        api.clear_histories()
        api.send_command(f"{UPDATER}.UPDATE_IMAGE_FROM", [dest, str(wrong_crc)])
        # FlashWorker rejects the image on CRC mismatch (warning event).
        evt = api.await_event(
            f"{WORKER}.ImageFileCrcMismatch", timeout=UPDATE_TIMEOUT_S
        )
        assert evt is not None, "expected ImageFileCrcMismatch on wrong-CRC update"
    finally:
        try:
            api.send_command(f"{FILE_MANAGER}.RemoveFile", [dest, "true"])
        except Exception:
            pass
