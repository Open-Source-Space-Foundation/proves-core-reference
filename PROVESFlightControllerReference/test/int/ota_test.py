"""
ota_test.py:

End-to-end integration test for over-the-air (OTA) firmware updates.

The satellite boots through MCUboot in swap mode. Flash is split into a
bootloader partition, a primary slot (``slot0_partition``, the running image)
and a secondary slot (``slot1_partition``, the staging area). An update is:

1. ``PREPARE_UPDATE``   -- erase the secondary slot.
2. file uplink          -- put the new signed image on the on-board filesystem.
3. ``UPDATE_IMAGE_FROM``-- copy that file into the secondary slot, CRC-checked.
4. ``CONFIGURE_NEXT_BOOT TEST`` -- mark the staged image for a one-shot trial.
5. reboot               -- MCUboot swaps the slots and runs the new image.
6. ``CONFIRM_UPDATE``   -- make the swap permanent. Without this, the *next*
   reboot reverts to the previous image.

The whole module is marked ``ota`` and is excluded from the default integration
run: it erases a flash slot, uplinks a ~1.4 MB file, and reboots the board.
Run it deliberately:

    make test-integration TEST=ota_test.py FILTER=ota

By default it re-flashes the image in ``build-artifacts/zephyr.signed.bin``,
i.e. the build sitting in the working tree. Point somewhere else with
``--ota-image``. The test proves a real swap happened by reading the project
version out of the image it uplinks and asserting the board reports that same
version after the reboot -- so uplinking a *different* build than the running
one makes the test strictly stronger.
"""

import json
import time
import zlib
from datetime import datetime
from pathlib import Path

import pytest
from common import cmdDispatch, proves_send_and_assert_command
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.models.serialize.time_type import TimeType
from fprime_gds.common.testing_fw import predicates
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# OTA severs the RF link (it reboots the board), so it only makes sense on UART.
pytestmark = [pytest.mark.uart_only, pytest.mark.ota]

updater = "Update.updater"
worker = "Update.worker"
version = "CdhCore.version"
fileManager = "FileHandling.fileManager"

# Directory on the satellite filesystem that holds the staged image.
UPDATE_DIR = "/update"

# Uplink is the long pole. fprime-gds.yml sets file-uplink-chunk-size 204 and
# file-uplink-cooldown 0.400, so a 1.4 MB image is ~7100 chunks -> ~48 min at the
# repo default. Lower the cooldown (GDS_EXTRA_ARGS on `make gds-integration`) to
# go faster; this ceiling is sized so the default still fits.
UPLINK_TIMEOUT_S = 90 * 60
# The flash write walks the image in CONFIG_IMG_BLOCK_BUF_SIZE (512 B) chunks
# with a 5 ms settle delay per chunk, reading each chunk back off littlefs.
IMAGE_WRITE_TIMEOUT_S = 15 * 60
# Cold reset plus an MCUboot swap of two 1 MB slots, then a full FSW boot.
SWAP_REBOOT_TIMEOUT_S = 180
# Erasing the whole 1 MB staging slot is awaited by event rather than through
# proves_send_and_assert_command's ack window. Measured at 2.8 s on the CI cube.
PREPARE_TIMEOUT_S = 120

# The TC replay window silently drops any frame whose sequence number the board
# has already accepted, and the GDS has been observed emitting the same sequence
# number twice for the first two commands of a session. Every other command in
# the suite absorbs that through proves_send_and_assert_command's retries; the
# commands here are awaited by event instead, so they need their own. Retrying
# is keyed on the component's "started" event rather than its completion event,
# so a retry can never re-issue work that is already underway.
COMMAND_ATTEMPTS = 3
DISPATCH_TIMEOUT_S = 20

# Svc.Version declares its version strings as `string size 40`, so anything
# longer is truncated in flight before it reaches telemetry.
VERSION_STRING_SIZE = 40


def send_and_confirm_dispatch(
    fprime_test_api: IntegrationTestAPI,
    command: str,
    started_event: str,
    args: list[str] | None = None,
) -> None:
    """Send a command, retrying until the flight side reports it started.

    Clears histories before each attempt, so the caller can search from index 0
    for whatever the command goes on to emit.
    """
    for _ in range(COMMAND_ATTEMPTS):
        fprime_test_api.clear_histories()
        fprime_test_api.send_command(command, args or [])
        if (
            fprime_test_api.await_event(started_event, timeout=DISPATCH_TIMEOUT_S)
            is not None
        ):
            return
    raise AssertionError(
        f"{command} never reached the flight software in {COMMAND_ATTEMPTS} attempts"
    )


def await_outcome(
    fprime_test_api: IntegrationTestAPI, names: list[str], timeout: int
) -> EventData:
    """Await whichever of ``names`` lands first, searching the whole history.

    ``await_event`` coerces a non-predicate argument into an event-*ID*
    predicate, which is why ``satisfies_any`` of event_predicates silently never
    matches here; a member-of over translated IDs is the form that works.
    """
    ids = [fprime_test_api.translate_event_name(name) for name in names]
    return fprime_test_api.await_event(
        predicates.is_a_member_of(ids), timeout=timeout, start=0
    )


def prepare_update(fprime_test_api: IntegrationTestAPI) -> None:
    """Erase the staging slot and wait for the erase to report success."""
    send_and_confirm_dispatch(
        fprime_test_api, f"{updater}.PREPARE_UPDATE", f"{updater}.PrepareUpdate"
    )
    outcome = await_outcome(
        fprime_test_api,
        [f"{updater}.PrepareUpdateSucceeded", f"{updater}.PrepareUpdateFailed"],
        timeout=PREPARE_TIMEOUT_S,
    )
    assert outcome is not None, "PREPARE_UPDATE reported neither success nor failure"
    assert outcome.template.get_name() == "PrepareUpdateSucceeded", (
        f"PREPARE_UPDATE failed: {outcome.get_str()}"
    )


def fprime_crc32(path: Path) -> int:
    """CRC32 of a file in the convention ``Os::File::calculateCrc`` uses.

    F Prime seeds with 0xFFFFFFFF and does *not* apply the trailing XOR that
    zlib does, so the flight-side value is the complement of ``zlib.crc32``.
    This matches ``tools/bin/calculate-crc.py``; keeping the two in agreement is
    what stops ``UPDATE_IMAGE_FROM`` from failing with IMAGE_CRC_MISMATCH.
    """
    crc = 0
    with open(path, "rb") as handle:
        while chunk := handle.read(8192):
            crc = zlib.crc32(chunk, crc)
    return ~crc & 0xFFFFFFFF


def read_project_version(fprime_test_api: IntegrationTestAPI) -> str:
    """Ask the running image which project version it is."""
    proves_send_and_assert_command(fprime_test_api, f"{version}.VERSION", ["PROJECT"])
    event: EventData = fprime_test_api.assert_event(
        f"{version}.ProjectVersion", timeout=10
    )
    return str(event.args[0].val)


def assert_board_responsive(fprime_test_api: IntegrationTestAPI) -> None:
    """Fail loudly if the satellite has stopped answering commands."""
    proves_send_and_assert_command(fprime_test_api, f"{cmdDispatch}.CMD_NO_OP")


@pytest.fixture(scope="module")
def ota_image(request: pytest.FixtureRequest) -> Path:
    image = Path(request.config.getoption("--ota-image"))
    if not image.is_file():
        pytest.skip(
            f"OTA image {image} not found -- run 'make build' or pass --ota-image"
        )
    return image


@pytest.fixture(scope="module")
def expected_version(request: pytest.FixtureRequest, ota_image: Path) -> str:
    """The project version the board must report once ``ota_image`` is running.

    Taken from ``--ota-expect-version`` when given, otherwise from the
    ``version.json`` the F Prime build writes next to the image. Either way it
    is checked against the image bytes, so a stale version.json cannot quietly
    turn the post-reboot assertion into a no-op.
    """
    override = request.config.getoption("--ota-expect-version")
    if override:
        candidate = override
    else:
        version_json = (
            Path("build-fprime-automatic-zephyr") / "versions" / "version.json"
        )
        if not version_json.is_file():
            pytest.skip(
                f"{version_json} not found and --ota-expect-version not given; "
                "cannot tell which version the uplinked image should report"
            )
        candidate = json.loads(version_json.read_text())["project_version"]

    if candidate.encode() not in ota_image.read_bytes():
        pytest.skip(
            f"project version {candidate!r} does not appear in {ota_image}; the "
            "version metadata and the image are from different builds"
        )
    return candidate[: VERSION_STRING_SIZE - 1]


def test_01_prepare_update_keeps_the_board_alive(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """PREPARE_UPDATE must erase the staging slot, not the running one.

    The flash area erased here is resolved from the ``slot1_partition``
    devicetree label. Zephyr numbers flash areas by devicetree dependency
    ordinal, so a hardcoded ID silently retargets whenever a partition is added
    anywhere in the DT -- which is how PREPARE_UPDATE once erased the live
    firmware and bricked the board. Asserting the board still answers commands
    afterwards is the regression guard for exactly that.
    """
    prepare_update(fprime_test_api)

    assert_board_responsive(fprime_test_api)
    assert read_project_version(fprime_test_api), "board lost its version telemetry"


def test_02_update_image_without_prepare_is_rejected(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """A write that was not preceded by a successful PREPARE_UPDATE must fail.

    FlashWorker tracks the last successful step; writing into a slot that was
    never erased would produce a corrupt image that MCUboot may still try to
    swap in.
    """
    # test_01 left the worker in the PREPARE state, so consume it with a write
    # that cannot succeed (the file does not exist), then retry from IDLE.
    fprime_test_api.send_command(
        f"{updater}.UPDATE_IMAGE_FROM", [f"{UPDATE_DIR}/does-not-exist.bin", "0"]
    )
    fprime_test_api.await_event(f"{updater}.UpdateFailed", timeout=30)

    fprime_test_api.clear_histories()
    fprime_test_api.send_command(
        f"{updater}.UPDATE_IMAGE_FROM", [f"{UPDATE_DIR}/does-not-exist.bin", "0"]
    )
    assert (
        fprime_test_api.await_event(f"{worker}.NoImagePrepared", timeout=30) is not None
    ), "unprepared write was not rejected"

    assert_board_responsive(fprime_test_api)


def test_03_full_ota_cycle(
    fprime_test_api: IntegrationTestAPI,
    start_gds,
    ota_image: Path,
    expected_version: str,
):
    """Stage an image, swap to it across a reboot, and confirm it."""
    before = read_project_version(fprime_test_api)
    crc32 = fprime_crc32(ota_image)
    destination = f"{UPDATE_DIR}/{ota_image.name}"

    # 1. Erase the staging slot.
    prepare_update(fprime_test_api)

    # 2. Uplink the signed image. CreateDirectory is best-effort: /update may
    #    already exist from an earlier run, and fileManager errors on that.
    fprime_test_api.send_command(f"{fileManager}.CreateDirectory", [UPDATE_DIR])
    time.sleep(1)
    fprime_test_api.clear_histories()
    fprime_test_api.uplink_file(str(ota_image), destination)
    assert (
        fprime_test_api.await_event("FileReceived", timeout=UPLINK_TIMEOUT_S)
        is not None
    ), f"uplink of {ota_image} to {destination} never completed"

    # 3. Copy it into the staging slot. The CRC is checked flight-side before a
    #    single byte is written, so a mismatch here means the uplink was lossy.
    send_and_confirm_dispatch(
        fprime_test_api,
        f"{updater}.UPDATE_IMAGE_FROM",
        f"{updater}.Update",
        [destination, str(crc32)],
    )
    outcome = await_outcome(
        fprime_test_api,
        [f"{updater}.UpdateSucceeded", f"{updater}.UpdateFailed"],
        timeout=IMAGE_WRITE_TIMEOUT_S,
    )
    assert outcome is not None, (
        "image write to the staging slot reported neither success nor failure"
    )
    assert outcome.template.get_name() == "UpdateSucceeded", (
        f"image write to the staging slot failed: {outcome.get_str()}"
    )

    # 4. Arm the one-shot trial boot. TEST rather than PERMANENT so that a bad
    #    image reverts on the following reboot instead of stranding the board.
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api, f"{updater}.CONFIGURE_NEXT_BOOT", ["TEST"]
    )
    fprime_test_api.assert_event(f"{updater}.SetNextBoot", timeout=10)

    # 5. Reboot and let MCUboot perform the swap.
    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )
    fprime_test_api.send_command("ReferenceDeployment.resetManager.COLD_RESET")
    assert (
        fprime_test_api.await_event(
            f"{version}.FrameworkVersion", start=start, timeout=SWAP_REBOOT_TIMEOUT_S
        )
        is not None
    ), "board did not come back after the swap reboot"

    # 6. The swapped-in image must be the one we uplinked.
    after = read_project_version(fprime_test_api)
    assert after == expected_version, (
        f"running version {after!r} after the swap, expected {expected_version!r} "
        f"(was {before!r} before the update)"
    )

    # 7. Make it permanent. Skipping this would revert on the next reboot.
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(fprime_test_api, f"{updater}.CONFIRM_UPDATE")
    fprime_test_api.assert_event(f"{updater}.ConfirmBoot", timeout=10)


def test_04_survives_a_second_reboot(fprime_test_api: IntegrationTestAPI, start_gds):
    """After CONFIRM_UPDATE the image sticks -- no revert on the next boot.

    This is the assertion that separates a confirmed update from a trial one: an
    unconfirmed TEST image is rolled back by MCUboot here.
    """
    expected = read_project_version(fprime_test_api)

    start: TimeType = TimeType().set_datetime(
        datetime.now(), time_base=TimeType.TimeBase("TB_DONT_CARE")
    )
    fprime_test_api.send_command("ReferenceDeployment.resetManager.COLD_RESET")
    assert (
        fprime_test_api.await_event(
            f"{version}.FrameworkVersion", start=start, timeout=SWAP_REBOOT_TIMEOUT_S
        )
        is not None
    ), "board did not come back after the confirmation reboot"

    assert read_project_version(fprime_test_api) == expected, (
        "image reverted after reboot -- CONFIRM_UPDATE did not take"
    )
