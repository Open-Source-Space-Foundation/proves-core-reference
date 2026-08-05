"""Integration tests for the over-the-air update command surface.

These exercise the update state machine and its telemetry against real hardware without
completing a flash cycle. Nothing here sets the next boot or reboots the board, so a run
cannot leave the spacecraft staged to boot an unintended image.

The full prepare -> write -> TEST boot -> confirm cycle needs a power cycle and a human, and
lives in test/long/ rather than here.
"""

import pytest
from fprime_gds.common.testing_fw import predicates

UPDATER = "Update.updater"
WORKER = "Update.worker"

# A file name that cannot exist on the flight filesystem
MISSING_IMAGE = "/update/definitely_not_here.bin"


def send_and_assert_event(fprime_test_api, command, args, event, timeout=10):
    """Send a command and wait for the event it should produce.

    Args:
        fprime_test_api: the integration test API
        command: mnemonic of the command to send
        args: list of string arguments
        event: mnemonic of the event expected in response
        timeout: seconds to wait for the event

    Returns:
        the event data object that was received
    """
    fprime_test_api.send_and_assert_event(command, args, [event], timeout=timeout)


@pytest.mark.usefixtures("fprime_test_api")
class TestUpdateStateMachine:
    """The ordering rules an operator has to work within."""

    def test_update_without_prepare_is_rejected(self, fprime_test_api):
        """An update before a preparation must be refused rather than writing to flash.

        Writing an image into a slot that was never erased corrupts it, so the component
        refuses and says so.
        """
        fprime_test_api.send_and_assert_event(
            f"{UPDATER}.UPDATE_IMAGE_FROM",
            [MISSING_IMAGE, "0"],
            [f"{WORKER}.NoImagePrepared"],
            timeout=15,
        )

    def test_prepare_reports_completion(self, fprime_test_api):
        """Preparation erases a 1 MB slot and must report when it finishes.

        This is the slowest step in the sequence, so the operator needs the completion event
        to know the board is ready for an image rather than still erasing.
        """
        fprime_test_api.send_and_assert_event(
            f"{UPDATER}.PREPARE_UPDATE",
            [],
            [f"{UPDATER}.PrepareUpdateSucceeded"],
            timeout=60,
        )

    def test_missing_image_file_is_reported_as_a_failure(self, fprime_test_api):
        """A nonexistent image must fail loudly.

        Regression coverage for the defect where read and write failures returned OP_OK and
        the Updater announced UpdateSucceeded for an image that was never written.
        """
        fprime_test_api.send_and_assert_event(
            f"{UPDATER}.PREPARE_UPDATE",
            [],
            [f"{UPDATER}.PrepareUpdateSucceeded"],
            timeout=60,
        )
        fprime_test_api.send_and_assert_event(
            f"{UPDATER}.UPDATE_IMAGE_FROM",
            [MISSING_IMAGE, "0"],
            [f"{UPDATER}.UpdateFailed"],
            timeout=30,
        )

    def test_a_failed_open_does_not_cost_another_erase(self, fprime_test_api):
        """A mistyped file name must be retryable without re-erasing the slot.

        The failure never reached the flash, so the erased slot is still good. Re-running the
        update must not report UNPREPARED, which would force another 1 MB erase on orbit.
        """
        fprime_test_api.send_and_assert_event(
            f"{UPDATER}.PREPARE_UPDATE",
            [],
            [f"{UPDATER}.PrepareUpdateSucceeded"],
            timeout=60,
        )
        fprime_test_api.send_and_assert_event(
            f"{UPDATER}.UPDATE_IMAGE_FROM",
            [MISSING_IMAGE, "0"],
            [f"{UPDATER}.UpdateFailed"],
            timeout=30,
        )
        # The retry must fail on the file again, not on the sequence
        fprime_test_api.assert_event_count(0, f"{WORKER}.NoImagePrepared")
        fprime_test_api.send_and_assert_event(
            f"{UPDATER}.UPDATE_IMAGE_FROM",
            [MISSING_IMAGE, "0"],
            [f"{UPDATER}.UpdateFailed"],
            timeout=30,
        )
        fprime_test_api.assert_event_count(0, f"{WORKER}.NoImagePrepared")


@pytest.mark.usefixtures("fprime_test_api")
class TestUpdateTelemetry:
    """Telemetry an operator relies on when a pass ends mid-update."""

    def test_stage_is_reported_after_preparation(self, fprime_test_api):
        """The stage channel must survive a pass gap.

        An operator returning on the next orbit needs to know where the update stands without
        replaying the event history, so the stage is a channel and not only an event.
        """
        fprime_test_api.send_and_assert_command(
            f"{UPDATER}.PREPARE_UPDATE", [], max_delay=60
        )
        fprime_test_api.assert_telemetry(
            f"{WORKER}.UpdateStage",
            predicates.equal_to("PREPARED"),
            timeout=30,
        )

    def test_failure_status_is_retained(self, fprime_test_api):
        """The status of the last operation must remain queryable after it failed."""
        fprime_test_api.send_and_assert_command(
            f"{UPDATER}.PREPARE_UPDATE", [], max_delay=60
        )
        fprime_test_api.send_and_assert_command(
            f"{UPDATER}.UPDATE_IMAGE_FROM", [MISSING_IMAGE, "0"], max_delay=30
        )
        fprime_test_api.assert_telemetry(
            f"{WORKER}.LastUpdateStatus",
            predicates.equal_to("IMAGE_FILE_READ_ERROR"),
            timeout=30,
        )


@pytest.mark.usefixtures("fprime_test_api")
class TestImageAssembly:
    """Joining the numbered segments an image is uplinked in."""

    def test_zero_segments_is_rejected(self, fprime_test_api):
        """A zero segment count is meaningless and must be refused before any file work."""
        fprime_test_api.send_and_assert_event(
            f"{WORKER}.ASSEMBLE_IMAGE",
            ["/update/img", "0", "/update/candidate.bin", "0"],
            [f"{WORKER}.InvalidSegmentCount"],
            timeout=15,
        )

    def test_missing_segments_are_reported(self, fprime_test_api):
        """Assembly must fail on the first missing segment rather than writing a short image.

        A short image would still be flashed and would fail only at boot, so it has to be
        caught here.
        """
        fprime_test_api.send_and_assert_event(
            f"{WORKER}.ASSEMBLE_IMAGE",
            ["/update/no_such_prefix", "2", "/update/candidate.bin", "0"],
            [f"{WORKER}.AssembleFailed"],
            timeout=30,
        )


@pytest.mark.usefixtures("fprime_test_api")
class TestPatchApplication:
    """Reconstructing an image from a delta patch."""

    def test_missing_patch_is_reported(self, fprime_test_api):
        """A nonexistent patch file must fail rather than producing an empty image."""
        fprime_test_api.send_and_assert_event(
            f"{WORKER}.APPLY_PATCH",
            ["/update/no_such.patch", "/update/candidate.bin", "0"],
            [f"{WORKER}.PatchFailed"],
            timeout=30,
        )
