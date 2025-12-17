"""test_multi_cubes.py:

This file contains tests for the day-in-the-life functionality of the Proves system.

Note: run test 1 with -s so you can see the print statements

pytest test/test_edge_cases_day_in_the_life.py --deployment build-artifacts/zephyr/fprime-zephyr-deployment -k="test_06_safe_mode_tests_power" -s

"""

import time
from pathlib import Path

import serial
from fprime_gds.common.data_types.ch_data import ChData

ROOT = Path(__file__).parent.parent

mode_manager = "ReferenceDeployment.modeManager"
# Open up the serial port to the ground station
try:
    pass_through_serial = serial.Serial("/dev/ttyACM0", baudrate=115200)
except Exception:
    print("[WARNING] Failed to open serial port /dev/ttyACM0")
    pass_through_serial = None


def test_01_safe_mode_tests_power(fprime_test_api):
    """Test to run the safe mode tests

    Args:
        fprime_test_api: the fprime Test API object

    We are going to test the hardware entering safe mode, exiting safe mode

    Note: run this test with -s so you can see the print statements

    """

    # ensure lora is on and transmitting
    fprime_test_api.send_command("ReferenceDeployment.lora.TRANSMIT", ["ENABLED"])

    # com deplay set to small number
    fprime_test_api.send_command(
        "ReferenceDeployment.telemetryDelay.DIVIDER_PRM_SET", ["2"]
    )

    fprime_test_api.send_command(
        "ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET", ["2"]
    )

    # force mode manager to exit safe more, ensure we are in normal
    fprime_test_api.send_command(f"{mode_manager}.EXIT_SAFE_MODE")

    # Get the beacon packet
    fprime_test_api.send_and_assert_command(
        "CdhCore.tlmSend.SEND_PKT", ["1"], timeout=20
    )

    time.sleep(1)

    # Get the current mode
    mode_result: ChData = fprime_test_api.assert_telemetry(
        f"{mode_manager}.CurrentMode", start="NOW", timeout=3
    )
    current_mode = mode_result.get_val()
    assert current_mode == 2, "Should be in NORMAL mode"

    fprime_test_api.send_command(
        f"{mode_manager}.SAFEMODEENTRYVOLTAGE_PRM_SET", ["7.7"]
    )
    fprime_test_api.send_command(
        f"{mode_manager}.SAFEMODERECOVERYVOLTAGE_PRM_SET", ["7.8"]
    )

    print("\n\n\n\n\n\n")
    print(f"Current mode: {current_mode}")
    print(
        "You have 30 seconds to enter safe mode by lowering the voltage below 7.35V. It needs to be below 7.35V for 10 seconds to enter safe mode."
    )
    print("\n\n\n\n\n\n")

    # check for the entering safe mode event
    fprime_test_api.assert_event(f"{mode_manager}.EnteringSafeMode", timeout=40)

    # ensure the load switches are turned off

    # ensure the radio is back to baseline

    print("\n\n\n\n\n\n")
    print("Entered safe mode")
    print(
        "You have 30 seconds to exit safe mode by raising the voltage above 8.0V. It needs to be above 8.0V for 10 seconds to exit safe mode."
    )
    print("\n\n\n\n\n\n")

    # AutoSafeModeExit event
    fprime_test_api.assert_event(f"{mode_manager}.AutoSafeModeExit", timeout=40)


def test_02_safe_mode_tests_load_switches(fprime_test_api):
    """Test to run the safe mode tests
    to ensure the load switches are turned off
    """

    # ensure we are in normal mode
    fprime_test_api.send_and_assert_command(f"{mode_manager}.EXIT_SAFE_MODE")

    # turn on load switches for the faces 0-3
    fprime_test_api.send_command(f"{mode_manager}.LOAD_SWITCH_TURN_ON", ["0"])
    fprime_test_api.send_command(f"{mode_manager}.LOAD_SWITCH_TURN_ON", ["1"])
    fprime_test_api.send_command(f"{mode_manager}.LOAD_SWITCH_TURN_ON", ["2"])
    fprime_test_api.send_command(f"{mode_manager}.LOAD_SWITCH_TURN_ON", ["3"])

    # force safe mode
    fprime_test_api.send_and_assert_command(f"{mode_manager}.FORCE_SAFE_MODE")

    # check for loadswitch state changed events
    fprime_test_api.assert_event(
        "ReferenceDeployment.face0LoadSwitch.StatusChanged", timeout=40
    )
    fprime_test_api.assert_event(
        "ReferenceDeployment.face1LoadSwitch.StatusChanged", timeout=40
    )
    fprime_test_api.assert_event(
        "ReferenceDeployment.face2LoadSwitch.StatusChanged", timeout=40
    )
    fprime_test_api.assert_event(
        "ReferenceDeployment.face3LoadSwitch.StatusChanged", timeout=40
    )
