"""test_filesystem_resilience.py:

Filesystem power-loss resilience tests for FAT32 on 8MB flash.

These tests systematically exercise filesystem corruption scenarios by
interrupting write operations with COLD_RESET and checking post-reboot
filesystem health. FAT32 has no journaling, so resets during writes can cause:
  1. Filesystem completely unmountable
  2. Duplicate/orphaned files

This is a manual interactive test — run with -s to see print output:
    pytest test/test_filesystem_resilience.py --deployment build-artifacts/zephyr/fprime-zephyr-deployment -s

Prerequisites:
    - Board connected via USB, firmware flashed
    - GDS running (the test framework handles connection)
"""

import time

file_manager = "FileHandling.fileManager"
reset_manager = "ReferenceDeployment.resetManager"
fs_format = "ReferenceDeployment.fsFormat"
fs_space = "ReferenceDeployment.fsSpace"
cmd_dispatch = "CdhCore.cmdDisp"

TEST_DIR = "/resilience_test"
REBOOT_SLEEP = 10  # seconds to wait for USB re-enumeration after reset
RECONNECT_TIMEOUT = 30  # seconds to poll CMD_NO_OP waiting for GDS reconnect


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _wait_for_reconnect(fprime_test_api):
    """Poll CMD_NO_OP until GDS reconnects after a board reset.

    Mirrors the pattern from FprimeZephyrReference/test/int/conftest.py.
    """
    print(f"    Waiting {REBOOT_SLEEP}s for USB re-enumeration...")
    time.sleep(REBOOT_SLEEP)

    print(f"    Polling CMD_NO_OP (up to {RECONNECT_TIMEOUT}s)...")
    deadline = time.time() + RECONNECT_TIMEOUT
    while time.time() < deadline:
        try:
            fprime_test_api.send_and_assert_command(
                f"{cmd_dispatch}.CMD_NO_OP", timeout=5
            )
            print("    GDS reconnected.")
            fprime_test_api.clear_histories()
            return
        except Exception:
            time.sleep(1)

    raise RuntimeError("GDS did not reconnect within timeout after reset")


def _cold_reset(fprime_test_api):
    """Send COLD_RESET (non-blocking) then wait for reconnect."""
    print("    Sending COLD_RESET...")
    fprime_test_api.send_command(f"{reset_manager}.COLD_RESET")
    _wait_for_reconnect(fprime_test_api)


def _check_fs_mounted(fprime_test_api):
    """Verify filesystem is mounted by checking FsSpace telemetry.

    FsSpace runs on the 1Hz rate group, so values should appear quickly.
    Returns (free_space, total_space) or raises on failure.
    """
    print("    Checking FsSpace telemetry...")
    try:
        free = fprime_test_api.assert_telemetry(
            f"{fs_space}.FreeSpace", timeout=5
        )
        total = fprime_test_api.assert_telemetry(
            f"{fs_space}.TotalSpace", timeout=5
        )
        free_val = free.get_val()
        total_val = total.get_val()
        print(f"    FreeSpace={free_val}  TotalSpace={total_val}")
        assert total_val > 0, "TotalSpace is 0 — filesystem not mounted"
        assert free_val >= 0, "FreeSpace is negative"
        return free_val, total_val
    except Exception as e:
        print(f"    [FAIL] FsSpace check failed: {e}")
        raise


def _check_root_listing(fprime_test_api):
    """Verify root directory can be listed (filesystem mountable)."""
    print("    Listing root directory /...")
    fprime_test_api.clear_histories()
    try:
        fprime_test_api.send_and_assert_command(
            f"{file_manager}.ListDirectory", ["/"], timeout=10
        )
        fprime_test_api.assert_event(
            f"{file_manager}.ListDirectoryStarted", timeout=5
        )
        fprime_test_api.assert_event(
            f"{file_manager}.ListDirectorySucceeded", timeout=15
        )
        print("    Root listing succeeded.")
        return True
    except Exception as e:
        print(f"    [FAIL] Root listing failed: {e}")
        return False


def _check_directory_listing(fprime_test_api, path):
    """List a directory and print results. Returns True if listing succeeded."""
    print(f"    Listing directory {path}...")
    fprime_test_api.clear_histories()
    try:
        fprime_test_api.send_and_assert_command(
            f"{file_manager}.ListDirectory", [path], timeout=10
        )
        fprime_test_api.assert_event(
            f"{file_manager}.ListDirectoryStarted", timeout=5
        )
        fprime_test_api.assert_event(
            f"{file_manager}.ListDirectorySucceeded", timeout=15
        )
        print(f"    Directory listing for {path} succeeded.")
        return True
    except Exception as e:
        print(f"    [WARN] Directory listing for {path} failed: {e}")
        return False


def _check_file_size(fprime_test_api, path):
    """Get file size. Returns size (int) or None if file doesn't exist."""
    fprime_test_api.clear_histories()
    try:
        fprime_test_api.send_and_assert_command(
            f"{file_manager}.FileSize", [path], timeout=5
        )
        event = fprime_test_api.assert_event(
            f"{file_manager}.FileSizeSucceeded", timeout=5
        )
        size = event.args[1].val
        print(f"    FileSize({path}) = {size}")
        return size
    except Exception:
        print(f"    FileSize({path}) — file not found or error")
        return None


def _check_file_crc(fprime_test_api, path):
    """Calculate CRC of a file. Returns CRC value or None."""
    fprime_test_api.clear_histories()
    try:
        fprime_test_api.send_and_assert_command(
            f"{file_manager}.CalculateCrc", [path], timeout=10
        )
        event = fprime_test_api.assert_event(
            f"{file_manager}.CalculateCrcSucceeded", timeout=10
        )
        crc = event.args[1].val
        print(f"    CRC({path}) = {crc:#010x}")
        return crc
    except Exception:
        print(f"    CRC({path}) — failed or file not found")
        return None


def _run_health_checks(fprime_test_api, label=""):
    """Run all post-reset health checks. Returns True if filesystem is healthy."""
    print(f"\n  === Post-Reset Health Checks{' (' + label + ')' if label else ''} ===")
    healthy = True

    try:
        _check_fs_mounted(fprime_test_api)
    except Exception:
        healthy = False

    if not _check_root_listing(fprime_test_api):
        healthy = False

    if healthy:
        print(f"  === Health: {'PASS' if healthy else 'FAIL'} ===\n")
    else:
        print("  === Health: FAIL — filesystem may be corrupted ===\n")

    return healthy


def _format_filesystem(fprime_test_api):
    """Format the filesystem to recover from corruption."""
    print("    Formatting filesystem with FsFormat.FORMAT...")
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{fs_format}.FORMAT", timeout=30
    )
    fprime_test_api.assert_event(
        f"{fs_format}.FileSystemFormatted", timeout=30
    )
    print("    Filesystem formatted successfully.")


def _ensure_clean_state(fprime_test_api):
    """Ensure test directory exists and is clean."""
    # Remove test dir contents if it exists (best effort)
    fprime_test_api.send_command(
        f"{file_manager}.RemoveFile",
        [f"{TEST_DIR}/test_file.txt", True],
    )
    fprime_test_api.send_command(
        f"{file_manager}.RemoveFile",
        [f"{TEST_DIR}/append_target.txt", True],
    )
    fprime_test_api.send_command(
        f"{file_manager}.RemoveFile",
        [f"{TEST_DIR}/delete_me.txt", True],
    )
    fprime_test_api.send_command(
        f"{file_manager}.RemoveFile",
        [f"{TEST_DIR}/rapid_write.txt", True],
    )
    fprime_test_api.send_command(
        f"{file_manager}.RemoveFile",
        [f"{TEST_DIR}/large_file.txt", True],
    )
    fprime_test_api.send_command(
        f"{file_manager}.RemoveDirectory", [f"{TEST_DIR}/new_dir"]
    )
    fprime_test_api.send_command(
        f"{file_manager}.RemoveDirectory", [TEST_DIR]
    )
    time.sleep(0.5)

    # Create fresh test directory
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.CreateDirectory", [TEST_DIR], timeout=5
    )
    fprime_test_api.assert_event(
        f"{file_manager}.CreateDirectorySucceeded", timeout=5
    )


def _recover_if_needed(fprime_test_api):
    """Check filesystem health and format if corrupted. Returns True if recovery happened."""
    healthy = _run_health_checks(fprime_test_api)
    if not healthy:
        print("    Filesystem corrupted — reformatting to continue testing...")
        _format_filesystem(fprime_test_api)
        return True
    return False


# ---------------------------------------------------------------------------
# Test Scenarios
# ---------------------------------------------------------------------------


def test_00_setup(fprime_test_api):
    """Verify board is connected and filesystem is healthy before testing."""
    print("\n" + "=" * 60)
    print("FILESYSTEM RESILIENCE TEST SUITE")
    print("=" * 60)
    print("\nVerifying initial board state...")

    fprime_test_api.send_and_assert_command(
        f"{cmd_dispatch}.CMD_NO_OP", timeout=10
    )
    print("  Board is responding to commands.")

    healthy = _run_health_checks(fprime_test_api)
    assert healthy, "Filesystem is not healthy at start — consider reformatting first"

    _ensure_clean_state(fprime_test_api)
    print("  Test directory created. Ready for resilience tests.\n")


def test_01_reset_during_file_create(fprime_test_api):
    """Reset during file creation (AppendFile to new file → COLD_RESET).

    Tests what happens when the board resets while creating a new file.
    After reboot, check if the file exists (partial or complete) or if
    the filesystem is corrupted.
    """
    print("\n" + "=" * 60)
    print("TEST 01: Reset During File Create")
    print("=" * 60)

    _ensure_clean_state(fprime_test_api)
    target = f"{TEST_DIR}/test_file.txt"

    # Send file create command (non-blocking) then immediately reset
    print("\n  Step 1: Send AppendFile (create new file) + immediate COLD_RESET")
    fprime_test_api.send_command(
        f"{file_manager}.AppendFile", ["/prmDb.dat", target]
    )
    time.sleep(0.1)  # tiny delay to let the write start
    _cold_reset(fprime_test_api)

    # Health checks
    print("\n  Step 2: Post-reset checks")
    recovered = _recover_if_needed(fprime_test_api)

    if not recovered:
        # Check if the file exists (could be partial, complete, or absent)
        size = _check_file_size(fprime_test_api, target)
        if size is not None:
            print(f"  Result: File exists with size {size} (may be partial)")
            _check_file_crc(fprime_test_api, target)
        else:
            print("  Result: File does not exist (write was interrupted before commit)")

        _check_directory_listing(fprime_test_api, TEST_DIR)
    else:
        print("  Result: Filesystem was corrupted and reformatted")

    print("  TEST 01 COMPLETE\n")


def test_02_reset_during_file_append(fprime_test_api):
    """Reset during file append (append to existing file → COLD_RESET).

    Creates a file first, records its size, then interrupts an append
    operation. After reboot, checks whether the file grew partially,
    stayed the same, or the filesystem is corrupted.
    """
    print("\n" + "=" * 60)
    print("TEST 02: Reset During File Append")
    print("=" * 60)

    _ensure_clean_state(fprime_test_api)
    target = f"{TEST_DIR}/append_target.txt"

    # Create the file first
    print("\n  Step 1: Create initial file")
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.AppendFile", ["/prmDb.dat", target], timeout=10
    )
    fprime_test_api.assert_event(
        f"{file_manager}.AppendFileSucceeded", timeout=10
    )
    initial_size = _check_file_size(fprime_test_api, target)
    initial_crc = _check_file_crc(fprime_test_api, target)
    print(f"  Initial file: size={initial_size}, crc={initial_crc:#010x}" if initial_crc else f"  Initial file: size={initial_size}")

    # Append + immediate reset
    print("\n  Step 2: Send AppendFile (to existing file) + immediate COLD_RESET")
    fprime_test_api.send_command(
        f"{file_manager}.AppendFile", ["/prmDb.dat", target]
    )
    time.sleep(0.1)
    _cold_reset(fprime_test_api)

    # Health checks
    print("\n  Step 3: Post-reset checks")
    recovered = _recover_if_needed(fprime_test_api)

    if not recovered:
        final_size = _check_file_size(fprime_test_api, target)
        if final_size is not None:
            if final_size == initial_size:
                print(f"  Result: File unchanged (size={final_size}) — append was not committed")
            elif initial_size is not None and final_size > initial_size:
                print(f"  Result: File grew ({initial_size} → {final_size}) — partial or full append")
            else:
                print(f"  Result: File size={final_size} (unexpected)")
            _check_file_crc(fprime_test_api, target)
        else:
            print("  Result: File no longer exists — data loss")
    else:
        print("  Result: Filesystem was corrupted and reformatted")

    print("  TEST 02 COMPLETE\n")


def test_03_reset_during_directory_create(fprime_test_api):
    """Reset during directory creation (CreateDirectory → COLD_RESET).

    Tests what happens when the board resets while creating a directory.
    """
    print("\n" + "=" * 60)
    print("TEST 03: Reset During Directory Create")
    print("=" * 60)

    _ensure_clean_state(fprime_test_api)
    new_dir = f"{TEST_DIR}/new_dir"

    # Create directory + immediate reset
    print("\n  Step 1: Send CreateDirectory + immediate COLD_RESET")
    fprime_test_api.send_command(
        f"{file_manager}.CreateDirectory", [new_dir]
    )
    time.sleep(0.05)  # minimal delay
    _cold_reset(fprime_test_api)

    # Health checks
    print("\n  Step 2: Post-reset checks")
    recovered = _recover_if_needed(fprime_test_api)

    if not recovered:
        # Check if the directory exists by trying to list it
        exists = _check_directory_listing(fprime_test_api, new_dir)
        if exists:
            print("  Result: Directory was created successfully before reset")
        else:
            print("  Result: Directory does not exist (creation was interrupted)")

        _check_directory_listing(fprime_test_api, TEST_DIR)
    else:
        print("  Result: Filesystem was corrupted and reformatted")

    print("  TEST 03 COMPLETE\n")


def test_04_reset_during_file_delete(fprime_test_api):
    """Reset during file deletion (RemoveFile → COLD_RESET).

    Creates a file, then interrupts its deletion. After reboot, checks
    whether the file still exists, is gone, or the FS is corrupted.
    """
    print("\n" + "=" * 60)
    print("TEST 04: Reset During File Delete")
    print("=" * 60)

    _ensure_clean_state(fprime_test_api)
    target = f"{TEST_DIR}/delete_me.txt"

    # Create the file first
    print("\n  Step 1: Create file to delete")
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.AppendFile", ["/prmDb.dat", target], timeout=10
    )
    fprime_test_api.assert_event(
        f"{file_manager}.AppendFileSucceeded", timeout=10
    )
    initial_size = _check_file_size(fprime_test_api, target)
    print(f"  File created with size={initial_size}")

    # Delete + immediate reset
    print("\n  Step 2: Send RemoveFile + immediate COLD_RESET")
    fprime_test_api.send_command(
        f"{file_manager}.RemoveFile", [target, False]
    )
    time.sleep(0.05)
    _cold_reset(fprime_test_api)

    # Health checks
    print("\n  Step 3: Post-reset checks")
    recovered = _recover_if_needed(fprime_test_api)

    if not recovered:
        final_size = _check_file_size(fprime_test_api, target)
        if final_size is not None:
            print(f"  Result: File still exists (size={final_size}) — delete was interrupted")
        else:
            print("  Result: File was deleted before reset completed")

        _check_directory_listing(fprime_test_api, TEST_DIR)
    else:
        print("  Result: Filesystem was corrupted and reformatted")

    print("  TEST 04 COMPLETE\n")


def test_05_rapid_writes_then_reset(fprime_test_api):
    """Rapid repeated writes followed by reset.

    Sends many small write commands in quick succession, then resets.
    This maximizes the chance of catching the filesystem mid-update.
    """
    print("\n" + "=" * 60)
    print("TEST 05: Rapid Repeated Writes + Reset")
    print("=" * 60)

    _ensure_clean_state(fprime_test_api)
    target = f"{TEST_DIR}/rapid_write.txt"
    num_writes = 5

    # Fire off multiple appends rapidly, then reset
    print(f"\n  Step 1: Send {num_writes} rapid AppendFile commands + COLD_RESET")
    for i in range(num_writes):
        fprime_test_api.send_command(
            f"{file_manager}.AppendFile", ["/prmDb.dat", target]
        )
        # No sleep between commands — fire as fast as possible

    time.sleep(0.1)  # let at least one write start
    _cold_reset(fprime_test_api)

    # Health checks
    print("\n  Step 2: Post-reset checks")
    recovered = _recover_if_needed(fprime_test_api)

    if not recovered:
        size = _check_file_size(fprime_test_api, target)
        if size is not None:
            print(f"  Result: File exists with size={size}")
            print(f"  (Expected 0 to {num_writes} appends worth of data)")
            _check_file_crc(fprime_test_api, target)
        else:
            print("  Result: File does not exist")

        _check_directory_listing(fprime_test_api, TEST_DIR)
    else:
        print("  Result: Filesystem was corrupted and reformatted")

    print("  TEST 05 COMPLETE\n")


def test_06_large_file_write_then_reset(fprime_test_api):
    """Large file write interrupted by reset.

    Builds up a file via repeated appends (simulating a large write),
    then resets mid-way through the sequence.
    """
    print("\n" + "=" * 60)
    print("TEST 06: Large File Write + Reset (mid-way)")
    print("=" * 60)

    _ensure_clean_state(fprime_test_api)
    target = f"{TEST_DIR}/large_file.txt"
    total_appends = 10
    reset_after = 5  # reset after this many confirmed appends

    # Build up the file with confirmed appends first
    print(f"\n  Step 1: Build file with {reset_after} confirmed appends")
    for i in range(reset_after):
        fprime_test_api.clear_histories()
        fprime_test_api.send_and_assert_command(
            f"{file_manager}.AppendFile", ["/prmDb.dat", target], timeout=10
        )
        fprime_test_api.assert_event(
            f"{file_manager}.AppendFileSucceeded", timeout=10
        )
        print(f"    Append {i + 1}/{reset_after} confirmed")

    size_before = _check_file_size(fprime_test_api, target)
    crc_before = _check_file_crc(fprime_test_api, target)

    # Now fire remaining appends without waiting, then reset
    remaining = total_appends - reset_after
    print(f"\n  Step 2: Send {remaining} more AppendFile commands + COLD_RESET")
    for i in range(remaining):
        fprime_test_api.send_command(
            f"{file_manager}.AppendFile", ["/prmDb.dat", target]
        )

    time.sleep(0.5)  # let some writes begin
    _cold_reset(fprime_test_api)

    # Health checks
    print("\n  Step 3: Post-reset checks")
    recovered = _recover_if_needed(fprime_test_api)

    if not recovered:
        size_after = _check_file_size(fprime_test_api, target)
        crc_after = _check_file_crc(fprime_test_api, target)

        if size_after is not None and size_before is not None:
            if size_after == size_before:
                print(f"  Result: File unchanged ({size_before} bytes) — no unconfirmed writes persisted")
            elif size_after > size_before:
                print(f"  Result: File grew ({size_before} → {size_after} bytes) — some unconfirmed writes persisted")
            else:
                print(f"  Result: File SHRUNK ({size_before} → {size_after} bytes) — data corruption")

            if crc_before is not None and crc_after is not None:
                if crc_before == crc_after:
                    print("  CRC unchanged — confirmed data intact")
                else:
                    print("  CRC changed — file content modified")
        elif size_after is None:
            print("  Result: File no longer exists — data loss")

        _check_directory_listing(fprime_test_api, TEST_DIR)
    else:
        print("  Result: Filesystem was corrupted and reformatted")

    print("  TEST 06 COMPLETE\n")


def test_07_varied_timing_resets(fprime_test_api):
    """Reset with varied timing delays between write and reset.

    Runs the same write operation with different delays (0s, 0.1s, 0.5s, 1s)
    before resetting, to catch different phases of the write operation.
    """
    print("\n" + "=" * 60)
    print("TEST 07: Varied Timing Resets")
    print("=" * 60)

    delays = [0, 0.1, 0.5, 1.0]
    results = []

    for delay in delays:
        print(f"\n  --- Sub-test: delay={delay}s ---")

        # Ensure clean state (may need format if previous sub-test corrupted FS)
        try:
            _ensure_clean_state(fprime_test_api)
        except Exception:
            print("    Could not create test dir — formatting...")
            _format_filesystem(fprime_test_api)
            _ensure_clean_state(fprime_test_api)

        target = f"{TEST_DIR}/test_file.txt"

        # Write + delayed reset
        print(f"    Sending AppendFile, waiting {delay}s, then COLD_RESET")
        fprime_test_api.send_command(
            f"{file_manager}.AppendFile", ["/prmDb.dat", target]
        )
        if delay > 0:
            time.sleep(delay)
        _cold_reset(fprime_test_api)

        # Quick health check
        healthy = True
        try:
            _check_fs_mounted(fprime_test_api)
        except Exception:
            healthy = False

        if not healthy:
            result = "CORRUPTED"
            print("    Filesystem corrupted — reformatting...")
            _format_filesystem(fprime_test_api)
        else:
            root_ok = _check_root_listing(fprime_test_api)
            if not root_ok:
                result = "CORRUPTED"
                _format_filesystem(fprime_test_api)
            else:
                size = _check_file_size(fprime_test_api, target)
                if size is not None:
                    result = f"FILE_EXISTS (size={size})"
                else:
                    result = "FILE_ABSENT"

        results.append((delay, result))
        print(f"    Result: {result}")

    # Summary
    print("\n  === Timing Results Summary ===")
    for delay, result in results:
        print(f"    delay={delay}s → {result}")

    print("\n  TEST 07 COMPLETE\n")


def test_08_cleanup(fprime_test_api):
    """Clean up test directory and print final summary."""
    print("\n" + "=" * 60)
    print("CLEANUP & SUMMARY")
    print("=" * 60)

    # Best-effort cleanup
    print("\n  Cleaning up test directory...")
    try:
        _ensure_clean_state(fprime_test_api)
        fprime_test_api.send_command(
            f"{file_manager}.RemoveDirectory", [TEST_DIR]
        )
        time.sleep(0.5)
    except Exception:
        print("  Could not clean up — filesystem may need formatting")

    # Final health check
    print("\n  Final filesystem health check:")
    _run_health_checks(fprime_test_api)

    print("\n" + "=" * 60)
    print("ALL FILESYSTEM RESILIENCE TESTS COMPLETE")
    print("Review the output above to see which scenarios caused corruption.")
    print("=" * 60 + "\n")
