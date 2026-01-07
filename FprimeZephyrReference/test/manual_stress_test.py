#!/usr/bin/env python3
"""
manual_stress_test.py:

Interactive manual stress test script for PROVES CubeSat.
Provides menu-driven access to stress test scenarios.

Usage:
    python manual_stress_test.py

Prerequisites:
    - Hardware connected
    - GDS running: make gds
"""

import os
import sys
import time
from pathlib import Path

# Add int directory to path for common imports
sys.path.insert(0, str(Path(__file__).parent / "int"))

from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.executables.cli import StandardPipelineParser

# Component paths
MODE_MANAGER = "ReferenceDeployment.modeManager"
STARTUP_MANAGER = "ReferenceDeployment.startupManager"
DETUMBLE_MANAGER = "ReferenceDeployment.detumbleManager"
LORA = "ReferenceDeployment.lora"
CMD_SEQ = "ReferenceDeployment.cmdSeq"
PAYLOAD_SEQ = "ReferenceDeployment.payloadSeq"
SAFE_MODE_SEQ = "ReferenceDeployment.safeModeSeq"
UPDATER = "Update.updater"

FACE_LOAD_SWITCHES = [
    "ReferenceDeployment.face0LoadSwitch",
    "ReferenceDeployment.face1LoadSwitch",
    "ReferenceDeployment.face2LoadSwitch",
    "ReferenceDeployment.face3LoadSwitch",
    "ReferenceDeployment.face4LoadSwitch",
    "ReferenceDeployment.face5LoadSwitch",
]
PAYLOAD_LOAD_SWITCHES = [
    "ReferenceDeployment.payloadPowerLoadSwitch",
    "ReferenceDeployment.payloadBatteryLoadSwitch",
]
ALL_LOAD_SWITCHES = FACE_LOAD_SWITCHES + PAYLOAD_LOAD_SWITCHES


LOAD_SWITCH_CHANNELS = [
    "ReferenceDeployment.face0LoadSwitch.IsOn",
    "ReferenceDeployment.face1LoadSwitch.IsOn",
    "ReferenceDeployment.face2LoadSwitch.IsOn",
    "ReferenceDeployment.face3LoadSwitch.IsOn",
    "ReferenceDeployment.face4LoadSwitch.IsOn",
    "ReferenceDeployment.face5LoadSwitch.IsOn",
    "ReferenceDeployment.payloadPowerLoadSwitch.IsOn",
    "ReferenceDeployment.payloadBatteryLoadSwitch.IsOn",
]


def print_menu():
    """Display main menu"""
    print("\n" + "=" * 60)
    print("PROVES CubeSat Stress Test - Manual Script")
    print("=" * 60)
    print("\n--- Load Switches ---")
    print("1.  Turn ALL load switches ON")
    print("2.  Turn ALL load switches OFF")
    print("3.  Enable full system (switches + detumble + LoRa)")
    print("\n--- Sequences ---")
    print("4.  Run startup sequence")
    print("5.  Run 3 sequencers simultaneously")
    print("6.  Test quiescence mechanism (5 second)")
    print("11. Cancel all running sequences")
    print("\n--- OTA Update ---")
    print("7.  OTA: PREPARE_UPDATE")
    print("8.  OTA: CONFIGURE_NEXT_BOOT (TEST mode)")
    print("9.  OTA: CONFIRM_UPDATE")
    print("10. Full OTA cycle (requires image upload)")
    print("\n--- Safe Mode ---")
    print("12. Force safe mode")
    print("13. Exit safe mode")
    print("20. Test safe mode cycle (enter + verify + exit)")
    print("21. Test safe mode counter increment")
    print("22. Test safe mode turns off load switches")
    print("23. Test safe mode reason tracking")
    print("24. Test repeated state persistence (3 cycles)")
    print("\n--- Telemetry ---")
    print("14. Monitor telemetry (30 seconds)")
    print("15. Disable quiescence (ARMED=FALSE)")
    print("\n0.  Exit")
    print("=" * 60)


def send_command(api: IntegrationTestAPI, command: str, args: list = None):
    """Send a command and print result"""
    if args is None:
        args = []
    try:
        api.send_command(command, args)
        print(f"  [OK] {command}")
        return True
    except Exception as e:
        print(f"  [FAIL] {command}: {e}")
        return False


def turn_all_switches_on(api: IntegrationTestAPI):
    """Turn on all load switches"""
    print("\nTurning ON all load switches...")
    for switch in ALL_LOAD_SWITCHES:
        send_command(api, f"{switch}.TURN_ON")
        time.sleep(0.2)
    print("[DONE] All load switches ON")


def turn_all_switches_off(api: IntegrationTestAPI):
    """Turn off all load switches"""
    print("\nTurning OFF all load switches...")
    for switch in ALL_LOAD_SWITCHES:
        send_command(api, f"{switch}.TURN_OFF")
        time.sleep(0.2)
    print("[DONE] All load switches OFF")


def enable_full_system(api: IntegrationTestAPI):
    """Enable full system load"""
    print("\nEnabling full system...")

    turn_all_switches_on(api)

    print("\nEnabling detumble manager (AUTO mode)...")
    send_command(api, f"{DETUMBLE_MANAGER}.SET_MODE", ["AUTO"])

    print("Enabling LoRa transmit...")
    send_command(api, f"{LORA}.TRANSMIT", ["ENABLED"])

    print("\n[DONE] Full system enabled. Monitor power consumption!")
    print("Use option 14 to monitor telemetry or check GDS.")


def run_startup_sequence(api: IntegrationTestAPI):
    """Run startup sequence"""
    print("\nDisabling quiescence for test...")
    send_command(api, f"{STARTUP_MANAGER}.ARMED_PRM_SET", ["FALSE"])
    time.sleep(0.5)

    print("Running startup sequence (NO_BLOCK)...")
    send_command(api, f"{CMD_SEQ}.CS_RUN", ["/seq/startup.bin", "NO_BLOCK"])

    print("\n[INFO] Sequence started. Use option 11 to cancel when done.")


def run_three_sequencers(api: IntegrationTestAPI):
    """Run all three sequencers simultaneously"""
    print("\nDisabling quiescence...")
    send_command(api, f"{STARTUP_MANAGER}.ARMED_PRM_SET", ["FALSE"])
    time.sleep(0.5)

    print("\nStarting cmdSeq with startup.seq...")
    send_command(api, f"{CMD_SEQ}.CS_RUN", ["/seq/startup.bin", "NO_BLOCK"])

    print("Starting payloadSeq with camera_handler_1.seq...")
    send_command(api, f"{PAYLOAD_SEQ}.CS_RUN", ["/seq/camera_handler_1.bin", "NO_BLOCK"])

    print("Starting safeModeSeq with radio-fast.seq...")
    send_command(api, f"{SAFE_MODE_SEQ}.CS_RUN", ["/seq/radio-fast.bin", "NO_BLOCK"])

    print("\n[DONE] All 3 sequencers running concurrently.")
    print("Use option 11 to cancel all sequences.")


def test_quiescence(api: IntegrationTestAPI):
    """Test quiescence mechanism with short timeout"""
    print("\nSetting quiescence time to 5 seconds...")
    send_command(api, f"{STARTUP_MANAGER}.QUIESCENCE_TIME_PRM_SET", ["5", "0"])

    print("Arming quiescence (ARMED=TRUE)...")
    send_command(api, f"{STARTUP_MANAGER}.ARMED_PRM_SET", ["TRUE"])

    print("Requesting telemetry...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])

    print("Waiting 6 seconds for quiescence to complete...")
    time.sleep(6)

    print("Restoring default quiescence (45 minutes)...")
    send_command(api, f"{STARTUP_MANAGER}.QUIESCENCE_TIME_PRM_SET", ["2700", "0"])

    print("Disabling quiescence (ARMED=FALSE)...")
    send_command(api, f"{STARTUP_MANAGER}.ARMED_PRM_SET", ["FALSE"])

    print("[DONE] Quiescence test complete.")


def ota_prepare(api: IntegrationTestAPI):
    """Prepare for OTA update"""
    print("\nPreparing for OTA update...")
    send_command(api, f"{UPDATER}.PREPARE_UPDATE")
    print("[INFO] Check GDS events for PrepareUpdateSucceeded (may take time)")


def ota_configure_test(api: IntegrationTestAPI):
    """Configure next boot for TEST mode"""
    print("\nConfiguring next boot for TEST mode...")
    send_command(api, f"{UPDATER}.CONFIGURE_NEXT_BOOT", ["TEST"])
    print("[INFO] Check GDS events for SetNextBoot")


def ota_confirm(api: IntegrationTestAPI):
    """Confirm current update"""
    print("\nConfirming current update...")
    send_command(api, f"{UPDATER}.CONFIRM_UPDATE")
    print("[INFO] Check GDS events for ConfirmBoot")


def ota_full_cycle(api: IntegrationTestAPI):
    """Full OTA cycle with prompts"""
    print("\n" + "=" * 50)
    print("FULL OTA UPDATE CYCLE")
    print("=" * 50)
    print(
        """
Prerequisites:
1. Build MCUboot bootloader: make build-mcuboot
2. Build signed firmware (after initial MCUboot install)
3. Calculate CRC: ./tools/bin/calculate-crc.py build-artifacts/zephyr.signed.bin
4. Upload image to device via File Uplink: /update/zephyr.signed.bin

OTA Flow:
1. PREPARE_UPDATE        - Prepare flash for update
2. UPDATE_IMAGE_FROM     - Write image to flash (requires CRC)
3. CONFIGURE_NEXT_BOOT   - Set TEST mode (safe, allows reversion)
4. Reboot                - Manual power cycle or reset command
5. CONFIRM_UPDATE        - After reboot, confirm image is good
"""
    )

    proceed = input("Continue with OTA preparation? (y/n): ").strip().lower()
    if proceed != "y":
        print("Aborted.")
        return

    # Step 1: Prepare
    ota_prepare(api)
    print("\nWaiting for preparation to complete...")
    time.sleep(5)

    # Step 2: Check if user has uploaded the image
    crc = input(
        "\nEnter CRC32 of zephyr.signed.bin (hex, e.g., 0x12345678) or 'skip': "
    ).strip()

    if crc.lower() != "skip":
        try:
            crc_int = int(crc, 16)
            print(f"\nUpdating from /update/zephyr.signed.bin with CRC {hex(crc_int)}...")
            send_command(
                api,
                f"{UPDATER}.UPDATE_IMAGE_FROM",
                ["/update/zephyr.signed.bin", str(crc_int)],
            )
            print("Waiting for update to complete (this may take a while)...")
            time.sleep(30)
        except ValueError:
            print("[ERROR] Invalid CRC format. Skipping UPDATE_IMAGE_FROM.")

    # Step 3: Configure next boot
    ota_configure_test(api)

    print("\n" + "=" * 50)
    print("UPDATE PREPARED - READY FOR REBOOT")
    print("=" * 50)
    print(
        """
To complete the update:
1. Reboot the device (power cycle or reset command)
2. After reboot, run this script again
3. Select option 9 to confirm the update

If the new image fails, it will automatically revert after next reboot.
"""
    )


def cancel_all_sequences(api: IntegrationTestAPI):
    """Cancel all running sequences"""
    print("\nCanceling all sequences...")
    for seq in [CMD_SEQ, PAYLOAD_SEQ, SAFE_MODE_SEQ]:
        send_command(api, f"{seq}.CS_CANCEL")
    print("[DONE] All sequences canceled (if any were running).")


def force_safe_mode(api: IntegrationTestAPI):
    """Force safe mode"""
    print("\nForcing safe mode...")
    send_command(api, f"{MODE_MANAGER}.FORCE_SAFE_MODE")
    print("[DONE] System entering safe mode. All load switches will turn OFF.")


def exit_safe_mode(api: IntegrationTestAPI):
    """Exit safe mode"""
    print("\nExiting safe mode...")
    send_command(api, f"{MODE_MANAGER}.EXIT_SAFE_MODE")
    print("[DONE] System in normal mode.")


def monitor_telemetry(api: IntegrationTestAPI):
    """Monitor telemetry for 30 seconds"""
    print("\nMonitoring telemetry for 30 seconds...")
    print("Requesting packets every 5 seconds...\n")

    for i in range(6):
        print(f"--- Iteration {i + 1}/6 ---")
        send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])  # Mode/health
        send_command(api, "CdhCore.tlmSend.SEND_PKT", ["7"])  # IMU
        send_command(api, "CdhCore.tlmSend.SEND_PKT", ["9"])  # Load switches
        send_command(api, "CdhCore.tlmSend.SEND_PKT", ["11"])  # Power
        time.sleep(5)

    print("\n[DONE] Telemetry monitoring complete. Check GDS for values.")


def disable_quiescence(api: IntegrationTestAPI):
    """Disable quiescence"""
    print("\nDisabling quiescence (ARMED=FALSE)...")
    send_command(api, f"{STARTUP_MANAGER}.ARMED_PRM_SET", ["FALSE"])
    print("[DONE] Quiescence disabled.")


def get_telemetry_value(api: IntegrationTestAPI, channel: str, timeout: float = 5.0):
    """Get a telemetry value, returns None if not available"""
    try:
        result = api.assert_telemetry(channel, timeout=timeout)
        return result.get_val()
    except Exception as e:
        print(f"  [WARN] Could not get {channel}: {e}")
        return None


def test_safe_mode_cycle(api: IntegrationTestAPI):
    """Test safe mode entry and exit cycle"""
    print("\n" + "=" * 50)
    print("TEST: Safe Mode Cycle (Enter + Verify + Exit)")
    print("=" * 50)

    # Get initial mode
    print("\n1. Getting initial mode...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    time.sleep(1)
    initial_mode = get_telemetry_value(api, f"{MODE_MANAGER}.CurrentMode")
    print(f"   Initial mode: {initial_mode} (1=SAFE, 2=NORMAL)")

    # Enter safe mode
    print("\n2. Entering safe mode...")
    send_command(api, f"{MODE_MANAGER}.FORCE_SAFE_MODE")
    time.sleep(3)

    # Verify in safe mode
    print("\n3. Verifying safe mode...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    time.sleep(1)
    mode = get_telemetry_value(api, f"{MODE_MANAGER}.CurrentMode")
    if mode == 1:
        print("   [PASS] Mode is SAFE_MODE (1)")
    else:
        print(f"   [FAIL] Expected SAFE_MODE (1), got {mode}")

    # Exit safe mode
    print("\n4. Exiting safe mode...")
    send_command(api, f"{MODE_MANAGER}.EXIT_SAFE_MODE")
    time.sleep(2)

    # Verify back to normal
    print("\n5. Verifying normal mode...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    time.sleep(1)
    mode = get_telemetry_value(api, f"{MODE_MANAGER}.CurrentMode")
    if mode == 2:
        print("   [PASS] Mode is NORMAL (2)")
    else:
        print(f"   [FAIL] Expected NORMAL (2), got {mode}")

    print("\n[DONE] Safe mode cycle test complete.")


def test_safe_mode_counter(api: IntegrationTestAPI):
    """Test that SafeModeEntryCount increments"""
    print("\n" + "=" * 50)
    print("TEST: Safe Mode Counter Increment")
    print("=" * 50)

    # Get initial count
    print("\n1. Getting initial SafeModeEntryCount...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    time.sleep(1)
    initial_count = get_telemetry_value(api, f"{MODE_MANAGER}.SafeModeEntryCount")
    print(f"   Initial count: {initial_count}")

    # Enter safe mode
    print("\n2. Entering safe mode...")
    send_command(api, f"{MODE_MANAGER}.FORCE_SAFE_MODE")
    time.sleep(3)

    # Get new count
    print("\n3. Getting new SafeModeEntryCount...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    time.sleep(1)
    new_count = get_telemetry_value(api, f"{MODE_MANAGER}.SafeModeEntryCount")
    print(f"   New count: {new_count}")

    if initial_count is not None and new_count is not None:
        if new_count == initial_count + 1:
            print("   [PASS] Counter incremented by 1")
        else:
            print(f"   [FAIL] Expected {initial_count + 1}, got {new_count}")

    # Exit safe mode
    print("\n4. Exiting safe mode...")
    send_command(api, f"{MODE_MANAGER}.EXIT_SAFE_MODE")

    print("\n[DONE] Counter test complete.")


def test_safe_mode_load_switches(api: IntegrationTestAPI):
    """Test that safe mode turns off all load switches"""
    print("\n" + "=" * 50)
    print("TEST: Safe Mode Turns Off Load Switches")
    print("=" * 50)

    # Turn on some switches first
    print("\n1. Turning ON all load switches...")
    for switch in ALL_LOAD_SWITCHES:
        send_command(api, f"{switch}.TURN_ON")
        time.sleep(0.1)

    time.sleep(1)

    # Enter safe mode
    print("\n2. Entering safe mode...")
    send_command(api, f"{MODE_MANAGER}.FORCE_SAFE_MODE")
    time.sleep(3)

    # Check all switches are off
    print("\n3. Verifying all load switches are OFF...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["9"])
    time.sleep(1)

    all_off = True
    for channel in LOAD_SWITCH_CHANNELS:
        value = get_telemetry_value(api, channel)
        status = "OFF" if value in [0, "OFF"] else "ON"
        if status == "OFF":
            print(f"   [PASS] {channel.split('.')[-2]}: {status}")
        else:
            print(f"   [FAIL] {channel.split('.')[-2]}: {status} (expected OFF)")
            all_off = False

    if all_off:
        print("\n   [PASS] All load switches are OFF in safe mode")
    else:
        print("\n   [FAIL] Some load switches are still ON")

    # Exit safe mode
    print("\n4. Exiting safe mode...")
    send_command(api, f"{MODE_MANAGER}.EXIT_SAFE_MODE")

    print("\n[DONE] Load switch test complete.")


def test_safe_mode_reason(api: IntegrationTestAPI):
    """Test safe mode reason tracking"""
    print("\n" + "=" * 50)
    print("TEST: Safe Mode Reason Tracking")
    print("=" * 50)
    print("SafeModeReason: NONE=0, LOW_BATTERY=1, SYSTEM_FAULT=2,")
    print("                GROUND_COMMAND=3, EXTERNAL_REQUEST=4, LORA=5")

    # Enter safe mode via command
    print("\n1. Entering safe mode via FORCE_SAFE_MODE...")
    send_command(api, f"{MODE_MANAGER}.FORCE_SAFE_MODE")
    time.sleep(3)

    # Check reason is GROUND_COMMAND (3)
    print("\n2. Checking CurrentSafeModeReason...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    time.sleep(1)
    reason = get_telemetry_value(api, f"{MODE_MANAGER}.CurrentSafeModeReason")
    print(f"   Reason: {reason}")

    if reason == 3 or str(reason) == "GROUND_COMMAND":
        print("   [PASS] Reason is GROUND_COMMAND (3)")
    else:
        print(f"   [FAIL] Expected GROUND_COMMAND (3), got {reason}")

    # Exit safe mode
    print("\n3. Exiting safe mode...")
    send_command(api, f"{MODE_MANAGER}.EXIT_SAFE_MODE")
    time.sleep(2)

    # Check reason is cleared to NONE (0)
    print("\n4. Checking reason is cleared...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    time.sleep(1)
    reason = get_telemetry_value(api, f"{MODE_MANAGER}.CurrentSafeModeReason")
    print(f"   Reason: {reason}")

    if reason == 0 or str(reason) == "NONE":
        print("   [PASS] Reason is NONE (0)")
    else:
        print(f"   [FAIL] Expected NONE (0), got {reason}")

    print("\n[DONE] Reason tracking test complete.")


def test_state_persistence(api: IntegrationTestAPI):
    """Test repeated state persistence (3 safe mode cycles)"""
    print("\n" + "=" * 50)
    print("TEST: Repeated State Persistence (3 Cycles)")
    print("=" * 50)

    # Get initial count
    print("\n1. Getting initial SafeModeEntryCount...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    time.sleep(1)
    initial_count = get_telemetry_value(api, f"{MODE_MANAGER}.SafeModeEntryCount")
    print(f"   Initial count: {initial_count}")

    # Run 3 cycles
    for i in range(3):
        print(f"\n--- Cycle {i + 1}/3 ---")

        print("   Entering safe mode...")
        send_command(api, f"{MODE_MANAGER}.FORCE_SAFE_MODE")
        time.sleep(2)

        print("   Exiting safe mode...")
        send_command(api, f"{MODE_MANAGER}.EXIT_SAFE_MODE")
        time.sleep(2)

    # Verify count incremented by 3
    print("\n2. Getting final SafeModeEntryCount...")
    send_command(api, "CdhCore.tlmSend.SEND_PKT", ["1"])
    time.sleep(1)
    final_count = get_telemetry_value(api, f"{MODE_MANAGER}.SafeModeEntryCount")
    print(f"   Final count: {final_count}")

    if initial_count is not None and final_count is not None:
        expected = initial_count + 3
        if final_count == expected:
            print(f"   [PASS] Counter incremented by 3 ({initial_count} -> {final_count})")
        else:
            print(f"   [FAIL] Expected {expected}, got {final_count}")

    print("\n[DONE] State persistence test complete.")


def get_gds_port():
    """Get GDS port from environment or use default"""
    return os.environ.get("GDS_PORT", "50050")


def main():
    """Main entry point"""
    print("=" * 60)
    print("PROVES CubeSat Manual Stress Test Script")
    print("=" * 60)

    # Get deployment path
    root = Path(__file__).parent.parent.parent
    deployment = root / "build-artifacts" / "zephyr" / "fprime-zephyr-deployment"

    if not deployment.exists():
        print(f"[WARNING] Deployment directory not found: {deployment}")
        print("Make sure you've run 'make build' first.")

    gds_port = get_gds_port()
    print(f"\nConnecting to GDS at localhost:{gds_port}...")
    print("Make sure GDS is running: make gds")

    # Create API connection using StandardPipelineParser
    pipeline = None
    try:
        dict_path = deployment / "dict" / "ReferenceDeploymentTopologyDictionary.json"
        logs_path = deployment / "logs"

        # Create logs directory if it doesn't exist
        logs_path.mkdir(parents=True, exist_ok=True)

        # Build command-line style arguments for the parser
        cli_args = [
            "--dictionary", str(dict_path),
            "--logs", str(logs_path),
            "--file-storage-directory", str(deployment),
            "--tts-port", str(gds_port),
            "--tts-addr", "localhost",
        ]

        pipeline_parser = StandardPipelineParser()
        args, _, *_ = pipeline_parser.parse_args(
            StandardPipelineParser.CONSTITUENTS,
            arguments=cli_args,
            client=True
        )
        pipeline = pipeline_parser.pipeline_factory(args)

        api = IntegrationTestAPI(pipeline)
        api.setup()
        print("[OK] Connected to GDS\n")
    except Exception as e:
        print(f"[ERROR] Failed to connect to GDS: {e}")
        print("\nMake sure GDS is running:")
        print("  Terminal 1: make gds")
        print("  Terminal 2: python manual_stress_test.py")
        if pipeline is not None:
            try:
                pipeline.disconnect()
            except Exception:
                pass
        sys.exit(1)

    actions = {
        "1": lambda: turn_all_switches_on(api),
        "2": lambda: turn_all_switches_off(api),
        "3": lambda: enable_full_system(api),
        "4": lambda: run_startup_sequence(api),
        "5": lambda: run_three_sequencers(api),
        "6": lambda: test_quiescence(api),
        "7": lambda: ota_prepare(api),
        "8": lambda: ota_configure_test(api),
        "9": lambda: ota_confirm(api),
        "10": lambda: ota_full_cycle(api),
        "11": lambda: cancel_all_sequences(api),
        "12": lambda: force_safe_mode(api),
        "13": lambda: exit_safe_mode(api),
        "14": lambda: monitor_telemetry(api),
        "15": lambda: disable_quiescence(api),
        # Safe mode tests (from mode_manager_test.py)
        "20": lambda: test_safe_mode_cycle(api),
        "21": lambda: test_safe_mode_counter(api),
        "22": lambda: test_safe_mode_load_switches(api),
        "23": lambda: test_safe_mode_reason(api),
        "24": lambda: test_state_persistence(api),
    }

    try:
        while True:
            print_menu()
            choice = input("Select option: ").strip()

            if choice == "0":
                print("\nCleaning up...")
                turn_all_switches_off(api)
                cancel_all_sequences(api)
                print("Exiting...")
                break

            if choice in actions:
                try:
                    actions[choice]()
                except Exception as e:
                    print(f"[ERROR] {e}")
            else:
                print("[ERROR] Invalid option. Please try again.")

    except KeyboardInterrupt:
        print("\n\nInterrupted. Cleaning up...")
        turn_all_switches_off(api)
        cancel_all_sequences(api)

    finally:
        try:
            api.teardown()
        except Exception:
            pass
        try:
            if pipeline is not None:
                pipeline.disconnect()
        except Exception:
            pass

    print("Goodbye!")


if __name__ == "__main__":
    main()
