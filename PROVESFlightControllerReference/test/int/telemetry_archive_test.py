"""Hardware integration tests for passive telemetry archive and file downlink."""

import getpass
import time
from pathlib import Path

from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

archive = "ReferenceDeployment.telemetryArchive"
packetizer = "CdhCore.tlmSend"
file_downlink = "FileHandling.fileDownlink"


def _create_closed_segment(api: IntegrationTestAPI) -> str:
    proves_send_and_assert_command(api, f"{archive}.ENABLE")
    api.clear_histories()
    proves_send_and_assert_command(api, f"{packetizer}.SEND_PKT", [1])
    api.assert_event(f"{archive}.SegmentOpened", timeout=10)
    proves_send_and_assert_command(api, f"{archive}.CLOSE_SEGMENT")
    event = api.assert_event(f"{archive}.SegmentClosed", timeout=10)
    return str(event.args[0].val)


def test_archive_segment_downlinks(
    fprime_test_api: IntegrationTestAPI, start_gds, tmp_path: Path
):
    """A closed archive segment traverses the existing file-downlink stack.

    Run this test with ``--with-radio`` to exercise command uplink and file
    downlink over LoRa; without that option it exercises the UART path.
    """
    source = _create_closed_segment(fprime_test_api)
    destination = f"telemetry_archive_{tmp_path.name}.tlm"

    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_downlink}.SendFile",
        [source, destination],
        retries=5,
    )
    started = fprime_test_api.assert_event(f"{file_downlink}.SendStarted", timeout=30)
    assert started.args[1].val == source
    sent = fprime_test_api.assert_event(f"{file_downlink}.FileSent", timeout=120)
    assert sent.args[0].val == source
    assert sent.args[1].val == destination

    received = Path("/tmp") / getpass.getuser() / destination
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline and not received.exists():
        time.sleep(0.25)
    assert received.exists(), "GDS did not reconstruct the downlinked archive file"
    contents = received.read_bytes()
    assert contents.startswith(b"TLMARCH1\x00\x01\x00\x00")
    assert len(contents) > 12, "Downlinked archive contains no telemetry records"
    received.unlink()


def test_deployment_transition_closes_permanent_segment(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Persisting deployed state closes protected capture and selects rolling mode."""
    antenna = "ReferenceDeployment.antennaDeployer"
    proves_send_and_assert_command(
        fprime_test_api, f"{antenna}.SET_DEPLOYMENT_STATE", [True]
    )
    proves_send_and_assert_command(fprime_test_api, f"{archive}.GET_STATUS")
    status = fprime_test_api.assert_event(f"{archive}.ArchiveStatus", timeout=10)
    assert status.args[0].val == "ROLLING"
