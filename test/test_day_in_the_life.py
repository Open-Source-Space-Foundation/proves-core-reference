"""test_day_in_the_life.py:

This file contains tests for the day-in-the-life functionality of the Proves system.
"""

import subprocess
import time
from pathlib import Path

import serial
from common import proves_send_and_assert_command
from fprime_gds.common.data_types.ch_data import ChData
from fprime_gds.common.testing_fw import predicates

ROOT = Path(__file__).parent.parent

mode_manager = "ReferenceDeployment.modeManager"
# Open up the serial port to the ground station
try:
    pass_through_serial = serial.Serial("/dev/ttyACM0", baudrate=115200)
except Exception:
    print("[WARNING] Failed to open serial port /dev/ttyACM0")
    pass_through_serial = None


def test_00_compile_sequences(fprime_test_api):
    """Test set up all sequences

    This will compile all sequences in preparation for the tests. This delegates to the the make-sequences scripts
    command.

    These tests assume all of these sequences are already uplinked, so please uplink them before running the tests.

    Args:
        fprime_test_api: the fprime Test API object (unused)
    """
    sequences_command = ROOT / "tools" / "bin" / "make-sequences"
    subprocess.run(sequences_command, check=True)


def test_01_enable_transmit(fprime_test_api):
    """Test to enable LoRa transmit

    Args:
        fprime_test_api: the fprime Test API object
    """
    fprime_test_api.send_command("ReferenceDeployment.lora.TRANSMIT", ["ENABLED"])


def test_02_run_radio_sequences(fprime_test_api):
    """Test to run radio-fast sequence

    WARNING: this assumes that the radio sequence has been uplinked already!

    Args:
        fprime_test_api: the fprime Test API object
    """
    fprime_test_api.send_command(
        "ReferenceDeployment.cmdSeq.CS_RUN", ["/seq/radio-fast.bin", "BLOCK"]
    )
    time.sleep(1)
    if pass_through_serial is not None:
        pass_through_serial.write(b"7\r\n")
    fprime_test_api.assert_event(
        "ReferenceDeployment.downlinkDelay.DividerSet", [0], timeout=20
    )


def test_03_uplink_sequences(fprime_test_api):
    """Test to uplink sequence files

    This test will uplink the sequence file binaries that are needed to run the remainder of the day in the life
    operations. This includes the: radio-fast.bin and camera_manager_1.bin

    Args:
        fprime_test_api: the fprime Test API object
    """

    fprime_test_api.send_command("FileHandling.fileManager.CreateDirectory", ["/seq"])
    for sequence in ["camera_handler_1.bin"]:
        # Remove sequence and then re-update
        fprime_test_api.send_command(
            "FileHandling.fileManager.RemoveFile", [f"/seq/{sequence}", True]
        )
        fprime_test_api.uplink_file_and_await_completion(
            f"{ROOT}/sequences/{sequence}", f"/seq/{sequence}", timeout=40
        )


def test_04_payload_sequence(fprime_test_api):
    """Test to run the camera manager sequence

    Run the camera handler sequence.

    Args:
        fprime_test_api: the fprime Test API object
    """
    fprime_test_api.send_command(
        "ReferenceDeployment.payloadSeq.CS_RUN", ["/seq/camera_handler_1.bin", "BLOCK"]
    )
    fprime_test_api.assert_event(
        "ReferenceDeployment.payloadSeq.CS_SequenceComplete", timeout=200
    )


def test_05_return_radio_sequences(fprime_test_api):
    """Test to run radio-fast sequence

    Make sure the radio returns to baseline.

    Args:
        fprime_test_api: the fprime Test API object
    """
    if pass_through_serial is not None:
        pass_through_serial.write(b"8\r\n")
    greater_0 = predicates.greater_than(0)
    fprime_test_api.assert_telemetry(
        "ReferenceDeployment.startupManager.BootCount", greater_0, timeout=1000
    )


def test_06_safe_mode_tests_power(fprime_test_api):
    """Test to run the safe mode tests

    Args:
        fprime_test_api: the fprime Test API object

    We are going to test the hardware entering safe mode, exiting safe mode

    """

    # Get the beacon packet
    proves_send_and_assert_command(fprime_test_api, "CdhCore.tlmSend.SEND_PKT", ["1"])

    # Get the current mode
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{mode_manager}.CurrentMode", start="NOW", timeout=3
    )
    current_mode = mode_result.get_val()
    assert current_mode == 2, "Should be in NORMAL mode"

    print(f"Current mode: {current_mode}")
    print(
        "You have 30 seconds to enter safe mode by lowering the voltage below 6.7V. It needs to be below 6.7V for 10 seconds to enter safe mode."
    )

    # check for the entering safe mode event
    fprime_test_api.assert_event(f"{mode_manager}.EnteringSafeMode", timeout=40)

    print("Entered safe mode")
    print(
        "You have 30 seconds to exit safe mode by raising the voltage above 8.0V. It needs to be above 8.0V for 10 seconds to exit safe mode."
    )

    fprime_test_api.assert_event(f"{mode_manager}.ExitingSafeMode", timeout=40)


def test_07_safe_mode_tests_enter_exit(fprime_test_api):
    """Test to run the safe mode tests
    to ensure the ground system can exit safe mode
    """

    proves_send_and_assert_command(fprime_test_api, f"{mode_manager}.FORCE_SAFE_MODE")

    fprime_test_api.assert_event(f"{mode_manager}.EnteringSafeMode", timeout=40)

    proves_send_and_assert_command(
        fprime_test_api,
        f"{mode_manager}.EXIT_SAFE_MODE",
        events=[f"{mode_manager}.ExitingSafeMode"],
    )

    fprime_test_api.assert_event(f"{mode_manager}.ExitingSafeMode", timeout=40)
