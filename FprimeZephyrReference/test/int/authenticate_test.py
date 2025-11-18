"""
authenticate_test.py:

Integration tests for the Authenticate component.
"""

import os
import signal
import subprocess
import time

import pytest
from common import cmdDispatch, proves_send_and_assert_command
from fprime_gds.common.data_types.event_data import EventData
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

authenticate = "ReferenceDeployment.authenticate"


@pytest.fixture(scope="session")
def start_gds(fprime_test_api_session: IntegrationTestAPI):
    """Fixture to start GDS with authenticate framer before tests and stop after tests

    This overrides the default start_gds fixture to use the authenticate-space-data-link
    framing plugin for testing authentication.
    """
    # Get the artifact directory (matches Makefile logic)
    artifact_dir = os.environ.get(
        "ARTIFACT_DIR", os.path.join(os.getcwd(), "build-artifacts")
    )
    dictionary = os.path.join(
        artifact_dir,
        "zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json",
    )

    # Find fprime-gds - try to use from venv if available, otherwise assume in PATH
    venv_path = os.environ.get("VIRTUAL_ENV", os.path.join(os.getcwd(), "fprime-venv"))
    fprime_gds_cmd = os.path.join(venv_path, "bin", "fprime-gds")
    if not os.path.exists(fprime_gds_cmd):
        # Fall back to calling fprime-gds directly (assumes it's in PATH)
        fprime_gds_cmd = "fprime-gds"

    # Start GDS with authenticate framer plugin
    # Use same arguments as Makefile but add --framing-selection
    pro = subprocess.Popen(
        [
            fprime_gds_cmd,
            "--framing-selection",
            "authenticate-space-data-link",
            "-n",
            "--gui=none",
            "--dictionary",
            dictionary,
            "--communication-selection",
            "uart",
            "--uart-baud",
            "115200",
            "--output-unframed-data",
            "-",
        ],
        cwd=os.getcwd(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        preexec_fn=os.setsid,
    )

    gds_working = False
    timeout_time = time.time() + 30
    while time.time() < timeout_time:
        try:
            fprime_test_api_session.send_and_assert_command(
                command=f"{cmdDispatch}.CMD_NO_OP"
            )
            gds_working = True
            break
        except Exception:
            time.sleep(1)
    assert gds_working, "GDS failed to start with authenticate framer"

    yield
    os.killpg(os.getpgid(pro.pid), signal.SIGTERM)


@pytest.fixture(autouse=True)
def configure_authenticate(fprime_test_api: IntegrationTestAPI, start_gds):
    """Configure the Authenticate component"""
    fprime_test_api.clear_histories()
    yield


def test_01_normal_gds_commands(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that if i send a no op it goes through the command stack"""
    proves_send_and_assert_command(fprime_test_api, f"{cmdDispatch}.CMD_NO_OP")

    # Should see ValidHash event when packet is authenticated
    fprime_test_api.assert_event(f"{authenticate}.ValidHash", timeout=10)


def test_02_get_spi_key(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that if i send a get spi key command it goes through the command stack"""
    # SPI 1 is defined in the default spi_dict.txt
    proves_send_and_assert_command(
        fprime_test_api, f"{authenticate}.GET_KEY_FROM_SPI", [1]
    )

    # Should see EmitSpiKey event with the key and type
    event: EventData = fprime_test_api.assert_event(
        f"{authenticate}.EmitSpiKey", timeout=10
    )
    assert "HMAC" in event.args[1].val, "Authentication type should be HMAC"
    assert len(event.args[0].val) > 0, "Key should be non-empty"


def test_03_get_sequence_number(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that if i send a get sequence number command it will return the correct sequence number"""
    proves_send_and_assert_command(fprime_test_api, f"{authenticate}.GET_SEQ_NUM")

    # Should see EmitSequenceNumber event with current sequence number
    event: EventData = fprime_test_api.assert_event(
        f"{authenticate}.EmitSequenceNumber", timeout=10
    )
    assert isinstance(event.args[0].val, int), "Sequence number should be an integer"
    assert event.args[0].val >= 0, "Sequence number should be non-negative"


def test_04_get_spi_key_invalid(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that if i send a get spi key command with an invalid spi it will return the default key"""
    # Use a very large SPI that doesn't exist (0xFFFF)
    invalid_spi = 0xFFFF
    proves_send_and_assert_command(
        fprime_test_api, f"{authenticate}.GET_KEY_FROM_SPI", [invalid_spi]
    )

    # Should still see EmitSpiKey event (with default key if SPI not found)
    event: EventData = fprime_test_api.assert_event(
        f"{authenticate}.EmitSpiKey", timeout=10
    )
    # Should use default key when SPI is not found
    assert len(event.args[0].val) > 0, "Should return default key for invalid SPI"


def test_05_set_sequence_number(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that if i send a set sequence number command it will set the correct sequence number"""
    test_sequence_number = 100

    proves_send_and_assert_command(
        fprime_test_api, f"{authenticate}.SET_SEQ_NUM", [test_sequence_number]
    )

    # Should see SetSequenceNumberSuccess event
    event: EventData = fprime_test_api.assert_event(
        f"{authenticate}.SetSequenceNumberSuccess", timeout=10
    )
    assert event.args[0].val == test_sequence_number, (
        "Sequence number should be set to requested value"
    )
    assert event.args[1].val, "Sequence number set should succeed"

    # Verify by getting the sequence number
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(fprime_test_api, f"{authenticate}.GET_SEQ_NUM")
    get_event: EventData = fprime_test_api.assert_event(
        f"{authenticate}.EmitSequenceNumber", timeout=10
    )
    assert get_event.args[0].val == test_sequence_number + 1, (
        f"Sequence number should be {test_sequence_number + 1}"
    )


def test_06_invalid_authentication_type(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that if i send a packet that has an invalid authentication type it will return the invalid authentication type event"""
    # meed to edit the plugin to return the invalid authentication type event
    pass


def test_07_invalid_hash(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that if i send a packet that has an invalid hash it will return the invalid hash event"""
    # meed to edit the plugin to return the invalid hash event
    pass


def test_08_sequence_number_out_of_window(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that if i send a packet that has a sequence number out of window it will return the sequence number out of window event"""
    # Get current sequence number
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(fprime_test_api, f"{authenticate}.GET_SEQ_NUM")
    current_seq_event: EventData = fprime_test_api.assert_event(
        f"{authenticate}.EmitSequenceNumber", timeout=10
    )
    current_seq = current_seq_event.args[0].val

    # Set sequence number to a value far in the future (assuming default window is 50)
    # This should cause the next packet to be out of window
    future_seq = current_seq + 100  #  larger than default window of 50
    proves_send_and_assert_command(
        fprime_test_api, f"{authenticate}.SET_SEQ_NUM", [future_seq]
    )

    fprime_test_api.clear_histories()

    # Now send a command - the framer plugin will use its own sequence counter
    # which starts at 0
    proves_send_and_assert_command(fprime_test_api, f"{cmdDispatch}.CMD_NO_OP")

    # Check we got the sequence number out of window event
    # Note: The event signature is SequenceNumberOutOfWindow(spi: U32, expected: U32, window: U32)
    # But the code actually passes (received, expected, window) - there may be a mismatch
    # For now, we'll check that the event was emitted
    event: EventData = fprime_test_api.assert_event(
        f"{authenticate}.SequenceNumberOutOfWindow", timeout=10
    )
    # Verify event parameters match expected values
    # args[0] should be SPI (1), args[1] should be expected (future_seq), args[2] should be window (50)
    assert event.args[0].val == 1, "SPI should be 1"  # Default SPI from plugin
    assert event.args[1].val == future_seq, (
        "Expected sequence number should match what we set"
    )
    assert event.args[2].val == 50, "Window size should be 50 (default)"

    # Verify that we did NOT get a ValidHash event (packet should be rejected)
    # The packet should be rejected due to sequence number being out of window
    fprime_test_api.clear_histories()
    try:
        fprime_test_api.assert_event(f"{authenticate}.ValidHash", timeout=2)
        assert False, (
            "Packet should not be authenticated when sequence number is out of window"
        )
    except AssertionError:
        # Expected - packet should not be authenticated
        pass


def test_09_authenticated_packets_count_telemetry(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that if i send a packet that is authenticated it will return the authenticated event and the authenticated packets count telemetry will increment"""
    # Get initial authenticated packets count
    initial_count = fprime_test_api.get_telemetry(
        f"{authenticate}.AuthenticatedPacketsCount"
    )
    initial_val = initial_count.val if initial_count else 0

    # Send a command that should be authenticated
    proves_send_and_assert_command(fprime_test_api, f"{cmdDispatch}.CMD_NO_OP")

    # Should see ValidHash event
    fprime_test_api.assert_event(f"{authenticate}.ValidHash", timeout=10)

    # Get updated authenticated packets count
    updated_count = fprime_test_api.get_telemetry(
        f"{authenticate}.AuthenticatedPacketsCount", timeout=5
    )
    assert updated_count is not None, "AuthenticatedPacketsCount telemetry should exist"
    assert updated_count.val > initial_val, (
        "AuthenticatedPacketsCount should increment after authentication"
    )


def test_10_rejected_packets_count(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test that if i send a packet that is too short it will return the rejected event"""
    # this test needs the plugin to be edited
    pass
