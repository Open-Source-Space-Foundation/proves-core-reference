"""test_day_in_the_life.py:

This file contains tests for the day-in-the-life functionality of the Proves system.
"""

import subprocess
from pathlib import Path

import serial

ROOT = Path(__file__).parent.parent

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
    if pass_through_serial is not None:
        pass_through_serial.write(b"7\r\n")
    fprime_test_api.assert_event(
        "ReferenceDeployment.downlinkDelay.DividerSet", [0], timeout=10
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


def test_05_payload_sequence(fprime_test_api):
    """Test to run the camera manager sequence

    Run the camera handler sequence.

    Args:
        fprime_test_api: the fprime Test API object
    """
    fprime_test_api.send_command(
        "ReferenceDeployment.payloadSeq.CS_RUN", ["/seq/camera_handler_1.bin", "BLOCK"]
    )
    fprime_test_api.assert_event(
        "ReferenceDeployment.payloadSeq.CS_SequenceComplete", timeout=90
    )


def test_06_return_radio_sequences(fprime_test_api):
    """Test to run radio-fast sequence

    Make sure the radio returns to baseline.

    Args:
        fprime_test_api: the fprime Test API object
    """
    if pass_through_serial is not None:
        pass_through_serial.write(b"8\r\n")
    fprime_test_api.assert_telemetry(
        "ReferenceDeployment.modeManager.CurrentMode", ["NORMAL"], timeout=1000
    )
