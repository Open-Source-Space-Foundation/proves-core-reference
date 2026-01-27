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


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def send_command(api: IntegrationTestAPI, command: str, args: list = None):
    """Send a command and print result."""
    if args is None:
        args = []
    try:
        api.send_command(command, args)
        print(f"  [OK] {command}")
        return True
    except Exception as e:
        print(f"  [FAIL] {command}: {e}")
        return False


def send_and_assert(
    api: IntegrationTestAPI, command: str, args: list = None, timeout: int = 10
):
    """Send a command and assert it completed."""
    if args is None:
        args = []
    api.send_and_assert_command(command, args, timeout=timeout)


def wait_for_reconnect(api: IntegrationTestAPI):
    """Poll CMD_NO_OP until GDS reconnects after a board reset.

    Mirrors the pattern from FprimeZephyrReference/test/int/conftest.py.
    """
    print(f"    Waiting {REBOOT_SLEEP}s for USB re-enumeration...")
    time.sleep(REBOOT_SLEEP)

    print(f"    Polling CMD_NO_OP (up to {RECONNECT_TIMEOUT}s)...")
    deadline = time.time() + RECONNECT_TIMEOUT
    while time.time() < deadline:
        try:
            api.send_and_assert_command(f"{CMD_DISPATCH}.CMD_NO_OP", timeout=5)
            print("    GDS reconnected.")
            api.clear_histories()
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
    """Verify filesystem is mounted by checking FsSpace telemetry.

    FsSpace runs on the 1Hz rate group, so values should appear quickly.
    Returns (free_space, total_space) or (None, None) on failure.
    """
    print("    Checking FsSpace telemetry...")
    try:
        free = api.assert_telemetry(f"{FS_SPACE}.FreeSpace", timeout=5)
        total = api.assert_telemetry(f"{FS_SPACE}.TotalSpace", timeout=5)
        free_val = free.get_val()
        total_val = total.get_val()
        print(f"    FreeSpace={free_val}  TotalSpace={total_val}")
        if total_val > 0 and free_val >= 0:
            return free_val, total_val
        print("    [FAIL] Invalid space values — filesystem may not be mounted")
        return None, None
    except Exception as e:
        print(f"    [FAIL] FsSpace check failed: {e}")
        return None, None


def check_root_listing(api: IntegrationTestAPI):
    """Verify root directory can be listed (filesystem mountable)."""
    print("    Listing root directory /...")
    api.clear_histories()
    try:
        api.send_and_assert_command(f"{FILE_MANAGER}.ListDirectory", ["/"], timeout=10)
        api.assert_event(f"{FILE_MANAGER}.ListDirectoryStarted", timeout=5)
        api.assert_event(f"{FILE_MANAGER}.ListDirectorySucceeded", timeout=15)
        print("    Root listing succeeded.")
        return True
    except Exception as e:
        print(f"    [FAIL] Root listing failed: {e}")
        return False


def check_directory_listing(api: IntegrationTestAPI, path: str):
    """List a directory and print results. Returns True if listing succeeded."""
    print(f"    Listing directory {path}...")
    api.clear_histories()
    try:
        api.send_and_assert_command(f"{FILE_MANAGER}.ListDirectory", [path], timeout=10)
        api.assert_event(f"{FILE_MANAGER}.ListDirectoryStarted", timeout=5)
        api.assert_event(f"{FILE_MANAGER}.ListDirectorySucceeded", timeout=15)
        print(f"    Directory listing for {path} succeeded.")
        return True
    except Exception as e:
        print(f"    [WARN] Directory listing for {path} failed: {e}")
        return False


def check_file_size(api: IntegrationTestAPI, path: str):
    """Get file size. Returns size (int) or None if file doesn't exist."""
    api.clear_histories()
    try:
        api.send_and_assert_command(f"{FILE_MANAGER}.FileSize", [path], timeout=5)
        event = api.assert_event(f"{FILE_MANAGER}.FileSizeSucceeded", timeout=5)
        size = event.args[1].val
        print(f"    FileSize({path}) = {size}")
        return size
    except Exception:
        print(f"    FileSize({path}) — file not found or error")
        return None


def check_file_crc(api: IntegrationTestAPI, path: str):
    """Calculate CRC of a file. Returns CRC value or None."""
    api.clear_histories()
    try:
        api.send_and_assert_command(f"{FILE_MANAGER}.CalculateCrc", [path], timeout=10)
        event = api.assert_event(f"{FILE_MANAGER}.CalculateCrcSucceeded", timeout=10)
        crc = event.args[1].val
        print(f"    CRC({path}) = {crc:#010x}")
        return crc
    except Exception:
        print(f"    CRC({path}) — failed or file not found")
        return None


def run_health_checks(api: IntegrationTestAPI, label: str = ""):
    """Run all post-reset health checks. Returns True if filesystem is healthy."""
    print(f"\n  === Post-Reset Health Checks{' (' + label + ')' if label else ''} ===")
    healthy = True

    free, total = check_fs_mounted(api)
    if free is None:
        healthy = False

    if not check_root_listing(api):
        healthy = False

    status = "PASS" if healthy else "FAIL — filesystem may be corrupted"
    print(f"  === Health: {status} ===\n")
    return healthy


def format_filesystem(api: IntegrationTestAPI):
    """Format the filesystem to recover from corruption.

    Format triggers asserts then kicks the watchdog, so you need ~26s for
    the watchdog to fire plus time to actually reboot and reformat.
    """
    print("    Formatting filesystem with FsFormat.FORMAT...")
    print(f"    (waiting up to {FORMAT_TIMEOUT}s — includes watchdog reset cycle)")
    api.clear_histories()
    api.send_and_assert_command(f"{FS_FORMAT}.FORMAT", timeout=FORMAT_TIMEOUT)
    api.assert_event(f"{FS_FORMAT}.FileSystemFormatted", timeout=FORMAT_TIMEOUT)
    print("    Filesystem formatted successfully.")


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
    time.sleep(0.5)

    api.clear_histories()
    api.send_and_assert_command(
        f"{FILE_MANAGER}.CreateDirectory", [TEST_DIR], timeout=5
    )
    api.assert_event(f"{FILE_MANAGER}.CreateDirectorySucceeded", timeout=5)


def recover_if_needed(api: IntegrationTestAPI):
    """Check filesystem health and format if corrupted. Returns True if recovery happened."""
    healthy = run_health_checks(api)
    if not healthy:
        print("    Filesystem corrupted — reformatting to continue testing...")
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

    print("\nVerifying board connection...")
    try:
        api.send_and_assert_command(f"{CMD_DISPATCH}.CMD_NO_OP", timeout=10)
        print("  Board is responding to commands.")
    except Exception as e:
        print(f"  [FAIL] Board not responding: {e}")
        return

    healthy = run_health_checks(api)
    if healthy:
        print("  Filesystem is healthy.")
    else:
        resp = input("  Filesystem unhealthy. Format now? (y/n): ").strip().lower()
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
    recovered = recover_if_needed(api)

    if not recovered:
        size = check_file_size(api, target)
        if size is not None:
            print(f"  Result: File exists with size {size} (may be partial)")
            check_file_crc(api, target)
        else:
            print("  Result: File does not exist (write was interrupted before commit)")
        check_directory_listing(api, TEST_DIR)
    else:
        print("  Result: Filesystem was corrupted and reformatted")

    print("\n  [DONE] Reset During File Create complete.")


def test_reset_during_file_append(api: IntegrationTestAPI):
    """Reset during file append (append to existing file -> COLD_RESET)."""
    print("\n" + "=" * 60)
    print("TEST: Reset During File Append")
    print("=" * 60)

    ensure_clean_state(api)
    target = f"{TEST_DIR}/append_target.txt"

    print("\n  Step 1: Create initial file")
    api.clear_histories()
    send_and_assert(api, f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])
    api.assert_event(f"{FILE_MANAGER}.AppendFileSucceeded", timeout=10)
    initial_size = check_file_size(api, target)
    initial_crc = check_file_crc(api, target)
    if initial_crc:
        print(f"  Initial file: size={initial_size}, crc={initial_crc:#010x}")
    else:
        print(f"  Initial file: size={initial_size}")

    print("\n  Step 2: Send AppendFile (to existing file) + immediate COLD_RESET")
    api.send_command(f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])
    time.sleep(0.1)
    if not cold_reset(api):
        return

    print("\n  Step 3: Post-reset checks")
    recovered = recover_if_needed(api)

    if not recovered:
        final_size = check_file_size(api, target)
        if final_size is not None:
            if final_size == initial_size:
                print(
                    f"  Result: File unchanged (size={final_size}) — append was not committed"
                )
            elif initial_size is not None and final_size > initial_size:
                print(
                    f"  Result: File grew ({initial_size} -> {final_size}) — partial or full append"
                )
            else:
                print(f"  Result: File size={final_size} (unexpected)")
            check_file_crc(api, target)
        else:
            print("  Result: File no longer exists — data loss")
    else:
        print("  Result: Filesystem was corrupted and reformatted")

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
    recovered = recover_if_needed(api)

    if not recovered:
        exists = check_directory_listing(api, new_dir)
        if exists:
            print("  Result: Directory was created successfully before reset")
        else:
            print("  Result: Directory does not exist (creation was interrupted)")
        check_directory_listing(api, TEST_DIR)
    else:
        print("  Result: Filesystem was corrupted and reformatted")

    print("\n  [DONE] Reset During Directory Create complete.")


def test_reset_during_file_delete(api: IntegrationTestAPI):
    """Reset during file deletion (RemoveFile -> COLD_RESET)."""
    print("\n" + "=" * 60)
    print("TEST: Reset During File Delete")
    print("=" * 60)

    ensure_clean_state(api)
    target = f"{TEST_DIR}/delete_me.txt"

    print("\n  Step 1: Create file to delete")
    api.clear_histories()
    send_and_assert(api, f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])
    api.assert_event(f"{FILE_MANAGER}.AppendFileSucceeded", timeout=10)
    initial_size = check_file_size(api, target)
    print(f"  File created with size={initial_size}")

    print("\n  Step 2: Send RemoveFile + immediate COLD_RESET")
    api.send_command(f"{FILE_MANAGER}.RemoveFile", [target, False])
    time.sleep(0.05)
    if not cold_reset(api):
        return

    print("\n  Step 3: Post-reset checks")
    recovered = recover_if_needed(api)

    if not recovered:
        final_size = check_file_size(api, target)
        if final_size is not None:
            print(
                f"  Result: File still exists (size={final_size}) — delete was interrupted"
            )
        else:
            print("  Result: File was deleted before reset completed")
        check_directory_listing(api, TEST_DIR)
    else:
        print("  Result: Filesystem was corrupted and reformatted")

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
    recovered = recover_if_needed(api)

    if not recovered:
        size = check_file_size(api, target)
        if size is not None:
            print(f"  Result: File exists with size={size}")
            print(f"  (Expected 0 to {num_writes} appends worth of data)")
            check_file_crc(api, target)
        else:
            print("  Result: File does not exist")
        check_directory_listing(api, TEST_DIR)
    else:
        print("  Result: Filesystem was corrupted and reformatted")

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

    print(f"\n  Step 1: Build file with {reset_after} confirmed appends")
    for i in range(reset_after):
        api.clear_histories()
        send_and_assert(api, f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])
        api.assert_event(f"{FILE_MANAGER}.AppendFileSucceeded", timeout=10)
        print(f"    Append {i + 1}/{reset_after} confirmed")

    size_before = check_file_size(api, target)
    crc_before = check_file_crc(api, target)

    remaining = total_appends - reset_after
    print(f"\n  Step 2: Send {remaining} more AppendFile commands + COLD_RESET")
    for i in range(remaining):
        api.send_command(f"{FILE_MANAGER}.AppendFile", ["/prmDb.dat", target])

    time.sleep(0.5)
    if not cold_reset(api):
        return

    print("\n  Step 3: Post-reset checks")
    recovered = recover_if_needed(api)

    if not recovered:
        size_after = check_file_size(api, target)
        crc_after = check_file_crc(api, target)

        if size_after is not None and size_before is not None:
            if size_after == size_before:
                print(
                    f"  Result: File unchanged ({size_before} bytes) — no unconfirmed writes persisted"
                )
            elif size_after > size_before:
                print(
                    f"  Result: File grew ({size_before} -> {size_after} bytes) — some unconfirmed writes persisted"
                )
            else:
                print(
                    f"  Result: File SHRUNK ({size_before} -> {size_after} bytes) — data corruption"
                )

            if crc_before is not None and crc_after is not None:
                if crc_before == crc_after:
                    print("  CRC unchanged — confirmed data intact")
                else:
                    print("  CRC changed — file content modified")
        elif size_after is None:
            print("  Result: File no longer exists — data loss")
        check_directory_listing(api, TEST_DIR)
    else:
        print("  Result: Filesystem was corrupted and reformatted")

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

        free, total = check_fs_mounted(api)
        if free is None:
            result = "CORRUPTED"
            print("    Filesystem corrupted — reformatting...")
            format_filesystem(api)
        else:
            root_ok = check_root_listing(api)
            if not root_ok:
                result = "CORRUPTED"
                format_filesystem(api)
            else:
                size = check_file_size(api, target)
                if size is not None:
                    result = f"FILE_EXISTS (size={size})"
                else:
                    result = "FILE_ABSENT"

        results.append((delay, result))
        print(f"    Result: {result}")

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

    # Repo root is 3 levels up: long/ -> test/ -> FprimeZephyrReference/ -> repo root
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
