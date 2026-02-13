#!/usr/bin/env python3
"""test_filesystem_resilience.py:

Interactive filesystem power-loss resilience test for FAT32 on 8MB flash.

Systematically exercises filesystem corruption scenarios by interrupting
write operations with COLD_RESET and checking post-reboot filesystem health.
FAT32 has no journaling, so resets during writes can cause:
  1. Filesystem completely unmountable
  2. Duplicate/orphaned files

Usage:
    python test_filesystem_resilience.py

Prerequisites:
    - Board connected via USB, firmware flashed
    - GDS running: make gds

Note:
    Standalone GDS client pipelines can send commands but may not receive
    events/telemetry via ZMQ. This script uses fire-and-forget commands
    with time-based waits (same pattern as manual_stress_test.py). For
    real-time event observation, watch the GDS GUI.
"""

import os
import sys
import time
from pathlib import Path

from fprime_gds.common.testing_fw.api import IntegrationTestAPI
from fprime_gds.executables.cli import StandardPipelineParser

# Component paths
FILE_MANAGER = "FileHandling.fileManager"
RESET_MANAGER = "ReferenceDeployment.resetManager"
FS_FORMAT = "ReferenceDeployment.fsFormat"
FS_SPACE = "ReferenceDeployment.fsSpace"
CMD_DISPATCH = "CdhCore.cmdDisp"

TEST_DIR = "/resilience_test"
REBOOT_SLEEP = 10  # seconds to wait for USB re-enumeration after reset
RECONNECT_TIMEOUT = 30  # seconds to poll CMD_NO_OP waiting for GDS reconnect
# Format triggers asserts then a watchdog reset (~26s watchdog timeout)
# plus time to actually reboot and reformat, so allow plenty of margin.
FORMAT_TIMEOUT = 60
# Time to wait for a command to complete on the board
CMD_WAIT = 2


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def get_telemetry_value(api: IntegrationTestAPI, channel: str, timeout: int = 5):
    """Try to read a telemetry channel value. Returns value or None.

    Standalone ZMQ client pipelines often cannot receive events/telemetry,
    so this always uses try/except with graceful fallback.
    """
    try:
        result = api.assert_telemetry(channel, timeout=timeout)
        return result.get_val()
    except Exception:
        return None


def wait_for_reconnect(api: IntegrationTestAPI):
    """Wait for board to reboot and GDS to reconnect after a reset.

    Sends CMD_NO_OP in a loop. Since standalone clients may not receive
    events, we use a time-based approach: send NO_OPs and assume the board
    is back once USB re-enumerates (~10s) and we can send without exception.
    """
    print(f"    Waiting {REBOOT_SLEEP}s for USB re-enumeration...")
    time.sleep(REBOOT_SLEEP)

    print(f"    Polling CMD_NO_OP (up to {RECONNECT_TIMEOUT}s)...")
    deadline = time.time() + RECONNECT_TIMEOUT
    while time.time() < deadline:
        try:
            api.send_command(f"{CMD_DISPATCH}.CMD_NO_OP")
            # If send_command succeeds, the transport layer is connected.
            # Wait a moment for the board to fully initialize.
            time.sleep(2)
            print("    GDS transport reconnected.")
            return True
        except Exception:
            time.sleep(1)

    print("    [FAIL] GDS did not reconnect within timeout after reset")
    return False


def cold_reset(api: IntegrationTestAPI):
    """Send COLD_RESET (non-blocking) then wait for reconnect."""
    print("    Sending COLD_RESET...")
    api.send_command(f"{RESET_MANAGER}.COLD_RESET")
    return wait_for_reconnect(api)


def check_fs_mounted(api: IntegrationTestAPI):
    """Check FsSpace telemetry to verify filesystem is mounted.

    Returns (free_space, total_space) or (None, None) if telemetry
    is unavailable (common for standalone clients).
    """
    print("    Checking FsSpace telemetry...")
    free_val = get_telemetry_value(api, f"{FS_SPACE}.FreeSpace")
    total_val = get_telemetry_value(api, f"{FS_SPACE}.TotalSpace")
    if free_val is not None and total_val is not None:
        print(f"    FreeSpace={free_val}  TotalSpace={total_val}")
        if total_val > 0 and free_val >= 0:
            return free_val, total_val
        print("    [FAIL] Invalid space values — filesystem may not be mounted")
        return None, None
    print("    [INFO] Telemetry not available (check GDS GUI for FsSpace values)")
    return None, None


def check_root_listing(api: IntegrationTestAPI):
    """Send ListDirectory / command and wait. Returns True (optimistic).

    Since standalone clients may not receive events, this sends the command
    and waits for it to complete. Check GDS GUI for ListDirectory events.
    """
    print("    Listing root directory /...")
    api.send_command(f"{FILE_MANAGER}.ListDirectory", ["/"])
    time.sleep(CMD_WAIT)
    print("    Root listing command sent (check GDS GUI for results).")
    return True


def check_directory_listing(api: IntegrationTestAPI, path: str):
    """Send ListDirectory command. Returns True (optimistic)."""
    print(f"    Listing directory {path}...")
    api.send_command(f"{FILE_MANAGER}.ListDirectory", [path])
    time.sleep(CMD_WAIT)
    print(f"    Directory listing command sent for {path}.")
    return True


def check_file_size(api: IntegrationTestAPI, path: str):
    """Send FileSize command. Returns None (events may not be received).

    Watch GDS GUI for FileSizeSucceeded or FileSizeError events.
    """
    api.send_command(f"{FILE_MANAGER}.FileSize", [path])
    time.sleep(CMD_WAIT)
    print(f"    FileSize({path}) command sent (check GDS GUI for result).")
    return None


def check_file_crc(api: IntegrationTestAPI, path: str):
    """Send CalculateCrc command. Returns None (events may not be received)."""
    api.send_command(f"{FILE_MANAGER}.CalculateCrc", [path])
    time.sleep(CMD_WAIT)
    print(f"    CalculateCrc({path}) command sent (check GDS GUI for result).")
    return None


def run_health_checks(api: IntegrationTestAPI, label: str = ""):
    """Run post-reset health checks by sending diagnostic commands.

    Since standalone clients may not receive events, this sends commands
    and reports what it can. The user should watch the GDS GUI for full results.
    """
    print(f"\n  === Post-Reset Health Checks{' (' + label + ')' if label else ''} ===")

    check_fs_mounted(api)
    check_root_listing(api)

    print("  === Health check commands sent (verify results in GDS GUI) ===\n")
    return True


def format_filesystem(api: IntegrationTestAPI):
    """Format the filesystem to recover from corruption.

    Format triggers asserts then kicks the watchdog, so you need ~26s for
    the watchdog to fire plus time to actually reboot and reformat.
    """
    print("    Formatting filesystem with FsFormat.FORMAT...")
    print(f"    (waiting {FORMAT_TIMEOUT}s — includes watchdog reset cycle)")
    api.send_command(f"{FS_FORMAT}.FORMAT")
    time.sleep(FORMAT_TIMEOUT)
    print("    Format command sent. Board should have rebooted.")
    print("    Waiting for reconnect...")
    wait_for_reconnect(api)
    print("    Format complete (verify in GDS GUI).")


def ensure_clean_state(api: IntegrationTestAPI):
    """Ensure test directory exists and is clean."""
    for fname in [
        "test_file.txt",
        "append_target.txt",
        "delete_me.txt",
        "rapid_write.txt",
        "large_file.txt",
    ]:
        api.send_command(f"{FILE_MANAGER}.RemoveFile", [f"{TEST_DIR}/{fname}", True])
    api.send_command(f"{FILE_MANAGER}.RemoveDirectory", [f"{TEST_DIR}/new_dir"])
    api.send_command(f"{FILE_MANAGER}.RemoveDirectory", [TEST_DIR])
    time.sleep(CMD_WAIT)

    api.send_command(f"{FILE_MANAGER}.CreateDirectory", [TEST_DIR])
    time.sleep(CMD_WAIT)
    print(f"    Test directory {TEST_DIR} created.")


def recover_if_needed(api: IntegrationTestAPI):
    """Run health checks and offer to format if needed."""
    run_health_checks(api)
    resp = input("    Filesystem OK? (y=continue / n=format): ").strip().lower()
    if resp == "n":
        print("    Reformatting to continue testing...")
        format_filesystem(api)
        return True
    return False


# ---------------------------------------------------------------------------
# Test Scenarios
# ---------------------------------------------------------------------------


def test_health_check(api: IntegrationTestAPI):
    """Verify board is connected and filesystem is healthy."""
    print("\n" + "=" * 60)
    print("HEALTH CHECK")
    print("=" * 60)

    print("\nSending CMD_NO_OP to verify board connection...")
    try:
        api.send_command(f"{CMD_DISPATCH}.CMD_NO_OP")
        time.sleep(CMD_WAIT)
        print("  CMD_NO_OP sent (check GDS GUI for OpCodeCompleted).")
    except Exception as e:
        print(f"  [FAIL] Could not send command: {e}")
        return

    run_health_checks(api)
    resp = input("  Format filesystem? (y/n): ").strip().lower()
    if resp == "y":
        format_filesystem(api)


def test_reset_during_file_create(api: IntegrationTestAPI):
    """Reset during file creation (AppendFile to new file -> COLD_RESET)."""
    print("\n" + "=" * 60)
    print("TEST: Reset During File Create")
    print("=" * 60)

    ensure_clean_state(api)
    target = f"{TEST_DIR}/test_file.txt"

    print("\n  Step 1: Send AppendFile (create new file) + immediate COLD_RESET")
    api.send_command(f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])
    time.sleep(0.1)
    if not cold_reset(api):
        return

    print("\n  Step 2: Post-reset checks")
    recover_if_needed(api)
    print("  Checking if file survived the reset:")
    check_file_size(api, target)
    check_file_crc(api, target)
    check_directory_listing(api, TEST_DIR)
    print("  Check GDS GUI: does the file exist? Is it partial?")

    print("\n  [DONE] Reset During File Create complete.")


def test_reset_during_file_append(api: IntegrationTestAPI):
    """Reset during file append (append to existing file -> COLD_RESET)."""
    print("\n" + "=" * 60)
    print("TEST: Reset During File Append")
    print("=" * 60)

    ensure_clean_state(api)
    target = f"{TEST_DIR}/append_target.txt"

    print("\n  Step 1: Create initial file")
    api.send_command(f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])
    time.sleep(CMD_WAIT)
    print("  Initial file created (check GDS GUI for AppendFileSucceeded).")
    check_file_size(api, target)
    check_file_crc(api, target)

    print("\n  Step 2: Send AppendFile (to existing file) + immediate COLD_RESET")
    api.send_command(f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])
    time.sleep(0.1)
    if not cold_reset(api):
        return

    print("\n  Step 3: Post-reset checks")
    recover_if_needed(api)
    print("  Checking file state after reset:")
    check_file_size(api, target)
    check_file_crc(api, target)
    check_directory_listing(api, TEST_DIR)
    print("  Check GDS GUI: did the file grow, stay same, or disappear?")

    print("\n  [DONE] Reset During File Append complete.")


def test_reset_during_directory_create(api: IntegrationTestAPI):
    """Reset during directory creation (CreateDirectory -> COLD_RESET)."""
    print("\n" + "=" * 60)
    print("TEST: Reset During Directory Create")
    print("=" * 60)

    ensure_clean_state(api)
    new_dir = f"{TEST_DIR}/new_dir"

    print("\n  Step 1: Send CreateDirectory + immediate COLD_RESET")
    api.send_command(f"{FILE_MANAGER}.CreateDirectory", [new_dir])
    time.sleep(0.05)
    if not cold_reset(api):
        return

    print("\n  Step 2: Post-reset checks")
    recover_if_needed(api)
    print("  Checking if directory survived the reset:")
    check_directory_listing(api, new_dir)
    check_directory_listing(api, TEST_DIR)
    print("  Check GDS GUI: does the directory exist?")

    print("\n  [DONE] Reset During Directory Create complete.")


def test_reset_during_file_delete(api: IntegrationTestAPI):
    """Reset during file deletion (RemoveFile -> COLD_RESET)."""
    print("\n" + "=" * 60)
    print("TEST: Reset During File Delete")
    print("=" * 60)

    ensure_clean_state(api)
    target = f"{TEST_DIR}/delete_me.txt"

    print("\n  Step 1: Create file to delete")
    api.send_command(f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])
    time.sleep(CMD_WAIT)
    print("  File created (check GDS GUI for AppendFileSucceeded).")
    check_file_size(api, target)

    print("\n  Step 2: Send RemoveFile + immediate COLD_RESET")
    api.send_command(f"{FILE_MANAGER}.RemoveFile", [target, False])
    time.sleep(0.05)
    if not cold_reset(api):
        return

    print("\n  Step 3: Post-reset checks")
    recover_if_needed(api)
    print("  Checking if file survived the reset:")
    check_file_size(api, target)
    check_directory_listing(api, TEST_DIR)
    print("  Check GDS GUI: does the file still exist or was it deleted?")

    print("\n  [DONE] Reset During File Delete complete.")


def test_rapid_writes_then_reset(api: IntegrationTestAPI):
    """Rapid repeated writes followed by reset."""
    print("\n" + "=" * 60)
    print("TEST: Rapid Repeated Writes + Reset")
    print("=" * 60)

    ensure_clean_state(api)
    target = f"{TEST_DIR}/rapid_write.txt"
    num_writes = 5

    print(f"\n  Step 1: Send {num_writes} rapid AppendFile commands + COLD_RESET")
    for i in range(num_writes):
        api.send_command(f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])

    time.sleep(0.1)
    if not cold_reset(api):
        return

    print("\n  Step 2: Post-reset checks")
    recover_if_needed(api)
    print(f"  Checking file state after {num_writes} rapid writes + reset:")
    check_file_size(api, target)
    check_file_crc(api, target)
    check_directory_listing(api, TEST_DIR)
    print("  Check GDS GUI for file state.")

    print("\n  [DONE] Rapid Writes + Reset complete.")


def test_large_file_write_then_reset(api: IntegrationTestAPI):
    """Large file write interrupted by reset."""
    print("\n" + "=" * 60)
    print("TEST: Large File Write + Reset (mid-way)")
    print("=" * 60)

    ensure_clean_state(api)
    target = f"{TEST_DIR}/large_file.txt"
    total_appends = 10
    reset_after = 5

    print(f"\n  Step 1: Build file with {reset_after} appends")
    for i in range(reset_after):
        api.send_command(f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])
        time.sleep(CMD_WAIT)
        print(f"    Append {i + 1}/{reset_after} sent")

    print("  Checking file state before additional writes:")
    check_file_size(api, target)
    check_file_crc(api, target)

    remaining = total_appends - reset_after
    print(f"\n  Step 2: Send {remaining} more AppendFile commands + COLD_RESET")
    for i in range(remaining):
        api.send_command(f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])

    time.sleep(0.5)
    if not cold_reset(api):
        return

    print("\n  Step 3: Post-reset checks")
    recover_if_needed(api)
    print("  Checking file state after partial write + reset:")
    check_file_size(api, target)
    check_file_crc(api, target)
    check_directory_listing(api, TEST_DIR)
    print("  Check GDS GUI: compare file size/CRC before vs after reset.")

    print("\n  [DONE] Large File Write + Reset complete.")


def test_varied_timing_resets(api: IntegrationTestAPI):
    """Reset with varied timing delays between write and reset."""
    print("\n" + "=" * 60)
    print("TEST: Varied Timing Resets")
    print("=" * 60)

    delays = [0, 0.1, 0.5, 1.0]
    results = []

    for delay in delays:
        print(f"\n  --- Sub-test: delay={delay}s ---")

        try:
            ensure_clean_state(api)
        except Exception:
            print("    Could not create test dir — formatting...")
            format_filesystem(api)
            ensure_clean_state(api)

        target = f"{TEST_DIR}/test_file.txt"

        print(f"    Sending AppendFile, waiting {delay}s, then COLD_RESET")
        api.send_command(f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])
        if delay > 0:
            time.sleep(delay)

        if not cold_reset(api):
            results.append((delay, "RECONNECT_FAILED"))
            continue

        check_fs_mounted(api)
        check_root_listing(api)
        check_file_size(api, target)

        result = input("    Result? (ok/corrupted/reconnect_failed): ").strip().lower()
        if result == "corrupted":
            print("    Reformatting...")
            format_filesystem(api)
            results.append((delay, "CORRUPTED"))
        elif result == "reconnect_failed":
            results.append((delay, "RECONNECT_FAILED"))
        else:
            results.append((delay, "OK"))

    print("\n  === Timing Results Summary ===")
    for delay, result in results:
        print(f"    delay={delay}s -> {result}")

    print("\n  [DONE] Varied Timing Resets complete.")


def run_all_tests(api: IntegrationTestAPI):
    """Run all test scenarios sequentially."""
    print("\n" + "=" * 60)
    print("RUNNING ALL TESTS")
    print("=" * 60)

    tests = [
        ("Reset During File Create", test_reset_during_file_create),
        ("Reset During File Append", test_reset_during_file_append),
        ("Reset During Directory Create", test_reset_during_directory_create),
        ("Reset During File Delete", test_reset_during_file_delete),
        ("Rapid Writes + Reset", test_rapid_writes_then_reset),
        ("Large File Write + Reset", test_large_file_write_then_reset),
        ("Varied Timing Resets", test_varied_timing_resets),
    ]

    results = []
    for name, test_fn in tests:
        print(f"\n>>> Starting: {name}")
        try:
            test_fn(api)
            results.append((name, "COMPLETED"))
        except Exception as e:
            print(f"  [ERROR] {name} failed: {e}")
            results.append((name, f"ERROR: {e}"))
            try:
                format_filesystem(api)
            except Exception:
                pass

    print("\n" + "=" * 60)
    print("ALL TESTS COMPLETE — SUMMARY")
    print("=" * 60)
    for name, status in results:
        print(f"  {name}: {status}")
    print()

    print("Cleaning up test directory...")
    try:
        ensure_clean_state(api)
        api.send_command(f"{FILE_MANAGER}.RemoveDirectory", [TEST_DIR])
        time.sleep(0.5)
    except Exception:
        print("  Could not clean up — filesystem may need formatting")

    run_health_checks(api, label="final")


def cleanup_test_dir(api: IntegrationTestAPI):
    """Clean up test directory."""
    print("\nCleaning up test directory...")
    try:
        ensure_clean_state(api)
        api.send_command(f"{FILE_MANAGER}.RemoveDirectory", [TEST_DIR])
        time.sleep(0.5)
        print("[DONE] Test directory cleaned up.")
    except Exception:
        print("[WARN] Could not clean up — filesystem may need formatting")


# ---------------------------------------------------------------------------
# Menu
# ---------------------------------------------------------------------------


def print_menu():
    """Display main menu."""
    print("\n" + "=" * 60)
    print("Filesystem Power-Loss Resilience Test")
    print("=" * 60)
    print("\n--- Individual Tests ---")
    print("1.  Reset during file create")
    print("2.  Reset during file append")
    print("3.  Reset during directory create")
    print("4.  Reset during file delete")
    print("5.  Rapid repeated writes + reset")
    print("6.  Large file write + reset (mid-way)")
    print("7.  Varied timing resets (0s, 0.1s, 0.5s, 1s)")
    print("\n--- Utilities ---")
    print("8.  Run ALL tests sequentially")
    print("9.  Health check (verify FS is mounted)")
    print("10. Format filesystem (FsFormat.FORMAT)")
    print("11. Clean up test directory")
    print("\n0.  Exit")
    print("=" * 60)


def get_gds_port():
    """Get GDS port from environment or use default."""
    return os.environ.get("GDS_PORT", "50050")


def main():
    """Main entry point."""
    print("=" * 60)
    print("Filesystem Power-Loss Resilience Test")
    print("=" * 60)
    print()
    print("FAT32 on 8MB flash has no journaling. This script tests")
    print("what happens when writes are interrupted by COLD_RESET.")
    print()

    # Repo root is 3 levels up: long/ -> test/ -> PROVESFlightControllerReference/ -> repo root
    root = Path(__file__).parent.parent.parent.parent
    deployment = root / "build-artifacts" / "zephyr" / "fprime-zephyr-deployment"

    if not deployment.exists():
        print(f"[WARNING] Deployment directory not found: {deployment}")
        print("Make sure you've run 'make build' first.")

    gds_port = get_gds_port()
    print(f"Connecting to GDS at localhost:{gds_port}...")
    print("Make sure GDS is running: make gds")

    pipeline = None
    try:
        dict_path = deployment / "dict" / "ReferenceDeploymentTopologyDictionary.json"
        logs_path = deployment / "logs"
        logs_path.mkdir(parents=True, exist_ok=True)

        cli_args = [
            "--dictionary",
            str(dict_path),
            "--logs",
            str(logs_path),
            "--file-storage-directory",
            str(deployment),
            "--tts-port",
            str(gds_port),
            "--tts-addr",
            "localhost",
        ]

        pipeline_parser = StandardPipelineParser()
        args, _, *_ = pipeline_parser.parse_args(
            StandardPipelineParser.CONSTITUENTS, arguments=cli_args, client=True
        )
        pipeline = pipeline_parser.pipeline_factory(args)

        api = IntegrationTestAPI(pipeline)
        api.setup()
        print("[OK] Connected to GDS\n")
    except Exception as e:
        print(f"[ERROR] Failed to connect to GDS: {e}")
        print("\nMake sure GDS is running:")
        print("  Terminal 1: make gds")
        print("  Terminal 2: python test_filesystem_resilience.py")
        if pipeline is not None:
            try:
                pipeline.disconnect()
            except Exception:
                pass
        sys.exit(1)

    actions = {
        "1": lambda: test_reset_during_file_create(api),
        "2": lambda: test_reset_during_file_append(api),
        "3": lambda: test_reset_during_directory_create(api),
        "4": lambda: test_reset_during_file_delete(api),
        "5": lambda: test_rapid_writes_then_reset(api),
        "6": lambda: test_large_file_write_then_reset(api),
        "7": lambda: test_varied_timing_resets(api),
        "8": lambda: run_all_tests(api),
        "9": lambda: test_health_check(api),
        "10": lambda: format_filesystem(api),
        "11": lambda: cleanup_test_dir(api),
    }

    try:
        while True:
            print_menu()
            choice = input("Select option: ").strip()

            if choice == "0":
                print("\nCleaning up...")
                cleanup_test_dir(api)
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
        cleanup_test_dir(api)

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
