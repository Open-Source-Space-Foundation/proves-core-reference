"""Radio integration test for the TlmArchive component."""

import os
import time
from pathlib import Path

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

pytestmark = [pytest.mark.radio_only]

TLM_ARCHIVE = "ReferenceDeployment.tlmArchive"
TELEMETRY_DELAY = "ReferenceDeployment.telemetryDelay"
FILE_MANAGER = "FileHandling.fileManager"
FILE_DOWNLINK = "FileHandling.fileDownlink"

DEFAULT_TELEMETRY_DIVIDER = 29
RECORD_TIMEOUT_S = 250
FILE_RECEIVE_TIMEOUT_S = 180


def _listed_tlm_files(fprime_test_api: IntegrationTestAPI) -> list[tuple[str, int]]:
    """List //tlm and return the telemetry filenames and sizes received as events."""
    # Directory listing entries are events sent separately from the command
    # response. Retry the listing when the lossy RF link drops all matching
    # entries even though the command response arrived.
    for _ in range(3):
        proves_send_and_assert_command(
            fprime_test_api,
            f"{FILE_MANAGER}.ListDirectory",
            ["//tlm"],
        )
        entries = []
        for event in fprime_test_api.get_event_test_history().retrieve():
            if (
                event.get_template().get_full_name()
                != f"{FILE_MANAGER}.DirectoryListing"
            ):
                continue
            directory, filename, size = (arg.val for arg in event.get_args())
            if directory == "//tlm" and filename.endswith(".tlm"):
                entries.append((filename, int(size)))
        if entries:
            return entries
        time.sleep(2)
    return []


def _await_complete_local_file(path: Path, expected_size: int) -> None:
    """Wait until GDS has received and closed the complete downlinked file."""
    deadline = time.monotonic() + FILE_RECEIVE_TIMEOUT_S
    while time.monotonic() < deadline:
        if path.is_file() and path.stat().st_size == expected_size:
            return
        time.sleep(1)
    actual_size = path.stat().st_size if path.exists() else "missing"
    pytest.fail(
        f"GDS did not save {path} at the expected size of {expected_size} bytes "
        f"(actual: {actual_size})"
    )


def test_tlm_archive_downlinks_record_over_radio(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Create a telemetry record, discover it, and downlink it over LoRa."""
    telemetry_delay_restored = False
    try:
        proves_send_and_assert_command(
            fprime_test_api,
            f"{TLM_ARCHIVE}.RECORDING_STATUS",
            ["ENABLED"],
        )
        proves_send_and_assert_command(
            fprime_test_api,
            f"{TELEMETRY_DELAY}.DIVIDER_PRM_SET",
            [3],
        )

        writing = fprime_test_api.await_event(
            f"{TLM_ARCHIVE}.Debug",
            args=["Writing record."],
            timeout=RECORD_TIMEOUT_S,
        )
        assert writing is not None, "TlmArchive did not start writing a record"

        # Stop the high-rate telemetry before sending filesystem commands or
        # file packets over the bandwidth-constrained, half-duplex radio link.
        # Waiting for this command's acknowledgement also gives the queued
        # telemetry generated at the fast rate time to drain.
        proves_send_and_assert_command(
            fprime_test_api,
            f"{TELEMETRY_DELAY}.DIVIDER_PRM_SET",
            [DEFAULT_TELEMETRY_DIVIDER],
        )
        telemetry_delay_restored = True

        # TlmArchive writes synchronously, but leave time for the filesystem
        # close and the remaining radio backlog to clear.
        time.sleep(5)

        entries = _listed_tlm_files(fprime_test_api)
        assert entries, "ListDirectory(//tlm) returned no telemetry archive files"
        source_name, expected_size = max(entries, key=lambda entry: entry[0])
        assert expected_size > 0, f"Telemetry archive {source_name} is empty"

        source_path = f"//tlm/{source_name}"
        destination_name = f"tlm_archive_{time.time_ns()}.tlm"
        artifact_dir = Path(
            os.environ.get(
                "TLM_ARCHIVE_ARTIFACT_DIR",
                "build-artifacts/radio-downlinked-tlm",
            )
        )
        local_path = artifact_dir / destination_name

        proves_send_and_assert_command(
            fprime_test_api,
            f"{FILE_DOWNLINK}.SendFile",
            [source_path, destination_name],
        )
        # The completed local file is the end-to-end assertion: unlike the
        # FileSent event, it proves every radio packet reached GDS.
        _await_complete_local_file(local_path, expected_size)
    finally:
        if not telemetry_delay_restored:
            proves_send_and_assert_command(
                fprime_test_api,
                f"{TELEMETRY_DELAY}.DIVIDER_PRM_SET",
                [DEFAULT_TELEMETRY_DIVIDER],
            )
