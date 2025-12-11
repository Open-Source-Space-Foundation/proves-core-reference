"""
file_manager_test.py:

Integration tests for the FileManager component.

Tests file handling commands:
- CreateDirectory (success and already-exists error)
- RemoveDirectory (success and not-empty error)
- MoveFile
- RemoveFile (success, with ignoreErrors flag, and error)
- AppendFile
- FileSize
- ListDirectory

Edge case characterization tests:
- Power cycle during large file upload (FAT filesystem behavior)
- Storage full during file upload (error handling and recovery)
"""

import os
import tempfile
import threading
import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

file_manager = "FileHandling.fileManager"
file_uplink = "FileHandling.fileUplink"
reset_manager = "ReferenceDeployment.resetManager"

# Test file size for large file tests (2MB)
LARGE_FILE_SIZE_BYTES = 2 * 1024 * 1024

# Test directory on the device filesystem
TEST_DIR = "/test_files"
TEST_SUBDIR = f"{TEST_DIR}/subdir"


@pytest.fixture(autouse=True)
def cleanup_test_files(fprime_test_api: IntegrationTestAPI, start_gds):
    """Clean up test files before and after each test"""
    # Cleanup before test
    _cleanup_test_directory(fprime_test_api)

    yield

    # Cleanup after test
    _cleanup_test_directory(fprime_test_api)


def _cleanup_test_directory(fprime_test_api: IntegrationTestAPI):
    """Helper to remove all test files and directories.

    Uses send_and_assert_command() with ignoreErrors=True for files (always succeeds),
    and send_command() for directories (may fail if non-existent). Waits for all
    commands to complete and clears histories to prevent interference with tests.
    """
    # Remove files first (ignoreErrors=True means command always succeeds)
    test_files = [
        f"{TEST_DIR}/test_file.txt",
        f"{TEST_DIR}/source.txt",
        f"{TEST_DIR}/dest.txt",
        f"{TEST_DIR}/moved.txt",
        f"{TEST_DIR}/appended.txt",
        f"{TEST_SUBDIR}/file.txt",
        # Edge test files
        f"{TEST_DIR}/large_upload_interrupted.bin",
        f"{TEST_DIR}/health_check.txt",
        f"{TEST_DIR}/should_fail_full.bin",
        f"{TEST_DIR}/recovery_check.txt",
    ]
    # Also clean up any filler files from storage full test
    for i in range(50):
        test_files.append(f"{TEST_DIR}/filler_{i:03d}.bin")
    for file_path in test_files:
        # With ignoreErrors=True, RemoveFile command succeeds even if file doesn't exist
        fprime_test_api.send_and_assert_command(
            f"{file_manager}.RemoveFile",
            [file_path, True],  # ignoreErrors=True
            timeout=5,
        )

    # Remove subdirectories (may fail if non-existent, that's ok)
    fprime_test_api.send_command(f"{file_manager}.RemoveDirectory", [TEST_SUBDIR])

    # Remove test directory (may fail if non-existent, that's ok)
    fprime_test_api.send_command(f"{file_manager}.RemoveDirectory", [TEST_DIR])

    # Wait for directory removal commands to complete before clearing histories
    time.sleep(0.5)

    # Clear histories so leftover events don't interfere with test assertions
    fprime_test_api.clear_histories()


def test_create_and_remove_directory(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test creating and removing a directory"""
    # Create directory
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )
    fprime_test_api.assert_event(
        f"{file_manager}.CreateDirectorySucceeded",
        timeout=5,
    )

    # Remove directory
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.RemoveDirectory",
        [TEST_DIR],
    )
    fprime_test_api.assert_event(
        f"{file_manager}.RemoveDirectorySucceeded",
        timeout=5,
    )


def test_file_size(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test getting the size of a file"""
    # First create the test directory
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )

    # Use AppendFile to create a file with known content
    # We'll append prmDb.dat to our test file to create it
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/test_file.txt"],
    )
    # File I/O operations may take longer on embedded flash storage
    fprime_test_api.assert_event(
        f"{file_manager}.AppendFileSucceeded",
        timeout=10,
    )

    # Get file size
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.FileSize",
        [f"{TEST_DIR}/test_file.txt"],
    )
    event = fprime_test_api.assert_event(
        f"{file_manager}.FileSizeSucceeded",
        timeout=5,
    )
    # Verify size is greater than 0
    assert event.args[1].val > 0, "File size should be greater than 0"


def test_move_file(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test moving a file from one location to another"""
    # Create test directory
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )

    # Create source file by appending from existing file
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/source.txt"],
    )

    # Move file
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.MoveFile",
        [f"{TEST_DIR}/source.txt", f"{TEST_DIR}/moved.txt"],
    )
    fprime_test_api.assert_event(
        f"{file_manager}.MoveFileSucceeded",
        timeout=5,
    )

    # Verify source no longer exists by checking FileSize fails
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(
        f"{file_manager}.FileSize",
        [f"{TEST_DIR}/source.txt"],
    )
    fprime_test_api.assert_event(
        f"{file_manager}.FileSizeError",
        timeout=5,
    )

    # Verify destination exists
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.FileSize",
        [f"{TEST_DIR}/moved.txt"],
    )
    fprime_test_api.assert_event(
        f"{file_manager}.FileSizeSucceeded",
        timeout=5,
    )


def test_append_file(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test appending one file to another"""
    # Create test directory
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )

    # Create first file
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/dest.txt"],
    )

    # Get initial size
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.FileSize",
        [f"{TEST_DIR}/dest.txt"],
    )
    initial_event = fprime_test_api.assert_event(
        f"{file_manager}.FileSizeSucceeded",
        timeout=5,
    )
    initial_size = initial_event.args[1].val

    # Append file to itself (doubles the content)
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/dest.txt"],
    )
    # File I/O operations may take longer on embedded flash storage
    fprime_test_api.assert_event(
        f"{file_manager}.AppendFileSucceeded",
        timeout=10,
    )

    # Verify size increased
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.FileSize",
        [f"{TEST_DIR}/dest.txt"],
    )
    final_event = fprime_test_api.assert_event(
        f"{file_manager}.FileSizeSucceeded",
        timeout=5,
    )
    final_size = final_event.args[1].val

    assert final_size > initial_size, (
        f"File size should have increased after append: {initial_size} -> {final_size}"
    )


def test_remove_file(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test removing a file"""
    # Create test directory
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )

    # Create file
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/test_file.txt"],
    )

    # Remove file
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.RemoveFile",
        [f"{TEST_DIR}/test_file.txt", False],  # ignoreErrors=False
    )
    fprime_test_api.assert_event(
        f"{file_manager}.RemoveFileSucceeded",
        timeout=5,
    )

    # Verify file no longer exists
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(
        f"{file_manager}.FileSize",
        [f"{TEST_DIR}/test_file.txt"],
    )
    fprime_test_api.assert_event(
        f"{file_manager}.FileSizeError",
        timeout=5,
    )


def test_list_directory(fprime_test_api: IntegrationTestAPI, start_gds):
    """Test listing directory contents"""
    # Create test directory
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )

    # Create a file in the directory
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/test_file.txt"],
    )

    # List directory
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.ListDirectory",
        [TEST_DIR],
    )

    # Verify listing started
    fprime_test_api.assert_event(
        f"{file_manager}.ListDirectoryStarted",
        timeout=5,
    )

    # Wait for directory listing to complete (rate-gated, may take multiple scheduler ticks)
    fprime_test_api.assert_event(
        f"{file_manager}.ListDirectorySucceeded",
        timeout=15,
    )


def test_remove_directory_not_empty_fails(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that removing a non-empty directory fails"""
    # Create test directory
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )

    # Create a file in the directory
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/test_file.txt"],
    )

    # Try to remove non-empty directory (should fail)
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(
        f"{file_manager}.RemoveDirectory",
        [TEST_DIR],
    )
    fprime_test_api.assert_event(
        f"{file_manager}.DirectoryRemoveError",
        timeout=5,
    )


def test_remove_file_nonexistent_with_ignore(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that removing a nonexistent file with ignoreErrors=True succeeds"""
    fprime_test_api.clear_histories()
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.RemoveFile",
        ["/nonexistent_file_12345.txt", True],  # ignoreErrors=True
    )
    # Should succeed without error event
    fprime_test_api.assert_event(
        f"{file_manager}.RemoveFileSucceeded",
        timeout=5,
    )


def test_remove_file_nonexistent_without_ignore_fails(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that removing a nonexistent file with ignoreErrors=False fails"""
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(
        f"{file_manager}.RemoveFile",
        ["/nonexistent_file_12345.txt", False],  # ignoreErrors=False
    )
    fprime_test_api.assert_event(
        f"{file_manager}.FileRemoveError",
        timeout=5,
    )


def test_create_directory_already_exists_fails(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Test that creating a directory that already exists fails"""
    # Create directory first
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )

    # Try to create same directory again (should fail)
    fprime_test_api.clear_histories()
    fprime_test_api.send_command(
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )
    fprime_test_api.assert_event(
        f"{file_manager}.DirectoryCreateError",
        timeout=5,
    )


# =============================================================================
# Edge Case Tests - Characterization tests for filesystem behavior
# =============================================================================


def _generate_test_file(size_bytes: int) -> str:
    """Generate a temporary test file with random-ish data.

    Returns the path to the generated file.
    """
    # Create temp file with pattern data (more compressible than random, faster to generate)
    fd, path = tempfile.mkstemp(suffix=".bin")
    try:
        # Write repeating pattern to reach desired size
        pattern = bytes(range(256)) * 4096  # 1MB pattern block
        bytes_written = 0
        while bytes_written < size_bytes:
            chunk_size = min(len(pattern), size_bytes - bytes_written)
            os.write(fd, pattern[:chunk_size])
            bytes_written += chunk_size
    finally:
        os.close(fd)
    return path


def _cleanup_large_test_files(fprime_test_api: IntegrationTestAPI, file_paths: list):
    """Clean up large test files from device filesystem."""
    for file_path in file_paths:
        fprime_test_api.send_and_assert_command(
            f"{file_manager}.RemoveFile",
            [file_path, True],  # ignoreErrors=True
            timeout=10,
        )
    time.sleep(0.5)
    fprime_test_api.clear_histories()


def test_edge_power_cycle_during_large_file_upload(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Edge case: Power cycle during large file upload.

    This is a CHARACTERIZATION TEST - it documents actual behavior rather than
    asserting expected behavior. The device uses FAT filesystem which is NOT
    power-loss resilient, so corruption is possible.

    Test procedure:
    1. Generate 2MB test file locally
    2. Start file upload in background thread
    3. After short delay, trigger WARM_RESET (simulating power loss)
    4. Wait for device reboot
    5. Observe and log filesystem state

    Observations captured:
    - Does partial file exist after reboot?
    - What is the file size if it exists?
    - Is the filesystem still functional?
    """
    dest_path = f"{TEST_DIR}/large_upload_interrupted.bin"
    upload_exception = None
    upload_complete = threading.Event()

    # Create test directory first
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )

    # Generate 2MB test file locally
    local_test_file = _generate_test_file(LARGE_FILE_SIZE_BYTES)

    try:
        # Start file upload in background thread
        def upload_file():
            """Background thread target for file upload."""
            nonlocal upload_exception
            try:
                fprime_test_api.uplink_file_and_await_completion(
                    local_test_file, dest_path, timeout=120
                )
            except Exception as e:
                upload_exception = e
            finally:
                upload_complete.set()

        upload_thread = threading.Thread(target=upload_file, daemon=True)
        upload_thread.start()

        # Wait for upload to start, then interrupt with reset
        # 500ms should be enough time to start transfer but not complete it
        time.sleep(0.5)

        print("\n" + "=" * 60)
        print("EDGE TEST: Power cycle during large file upload")
        print("=" * 60)
        print(
            f"Triggering WARM_RESET during upload of {LARGE_FILE_SIZE_BYTES} bytes..."
        )

        fprime_test_api.clear_histories()
        fprime_test_api.send_command(f"{reset_manager}.WARM_RESET")

        # Wait for device to reboot
        fprime_test_api.assert_event(
            "CdhCore.version.FrameworkVersion",
            timeout=15,
        )
        print("Device rebooted successfully.")

        # Wait a bit for filesystem to stabilize after reboot
        time.sleep(2)

        # Check if partial file exists
        fprime_test_api.clear_histories()
        fprime_test_api.send_command(
            f"{file_manager}.FileSize",
            [dest_path],
        )

        # Try to get file size - may succeed or fail
        time.sleep(2)

        # Check for either success or error event
        try:
            event = fprime_test_api.assert_event(
                f"{file_manager}.FileSizeSucceeded",
                timeout=5,
            )
            file_size = event.args[1].val
            print("\nOBSERVATION: Partial file EXISTS")
            print(f"  - File size: {file_size} bytes")
            print(f"  - Expected size: {LARGE_FILE_SIZE_BYTES} bytes")
            print(
                f"  - Percentage written: {100 * file_size / LARGE_FILE_SIZE_BYTES:.1f}%"
            )
        except AssertionError:
            # File doesn't exist - check for error event
            try:
                fprime_test_api.assert_event(
                    f"{file_manager}.FileSizeError",
                    timeout=2,
                )
                print("\nOBSERVATION: Partial file DOES NOT EXIST")
                print("  - File was either never created or cleaned up")
            except AssertionError:
                print("\nOBSERVATION: UNKNOWN - no response to FileSize command")

        # Verify filesystem is still functional by creating a small test file
        fprime_test_api.clear_histories()
        proves_send_and_assert_command(
            fprime_test_api,
            f"{file_manager}.AppendFile",
            ["/prmDb.dat", f"{TEST_DIR}/health_check.txt"],
        )
        try:
            fprime_test_api.assert_event(
                f"{file_manager}.AppendFileSucceeded",
                timeout=10,
            )
            print("\nOBSERVATION: Filesystem is FUNCTIONAL after reboot")
            print("  - Successfully created new file after power cycle")
        except AssertionError:
            print("\nOBSERVATION: Filesystem may be CORRUPTED")
            print("  - Failed to create new file after power cycle")

        print("=" * 60)

        # Cleanup
        _cleanup_large_test_files(
            fprime_test_api, [dest_path, f"{TEST_DIR}/health_check.txt"]
        )

    finally:
        # Clean up local test file
        if os.path.exists(local_test_file):
            os.remove(local_test_file)


def test_edge_storage_full_during_file_upload(
    fprime_test_api: IntegrationTestAPI, start_gds
):
    """Edge case: Attempt file upload when storage is full.

    This is a CHARACTERIZATION TEST - it documents actual behavior when
    attempting to write to a full filesystem.

    Test procedure:
    1. Create test directory
    2. Fill filesystem by creating multiple large files
    3. Attempt to upload another file
    4. Observe error behavior
    5. Clean up ALL filler files to restore filesystem

    Observations captured:
    - What error event is emitted?
    - Is a partial file left behind?
    - Does system recover after cleanup?
    """
    filler_files = []
    dest_path = f"{TEST_DIR}/should_fail_full.bin"

    # Create test directory
    proves_send_and_assert_command(
        fprime_test_api,
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
    )

    print("\n" + "=" * 60)
    print("EDGE TEST: Storage full during file upload")
    print("=" * 60)

    try:
        # Fill filesystem with filler files
        # Use AppendFile repeatedly to create large files on device
        # We'll keep appending until we get an error
        print("Filling filesystem with filler files...")

        filler_index = 0
        consecutive_failures = 0
        max_filler_files = 50  # Safety limit

        while filler_index < max_filler_files and consecutive_failures < 3:
            filler_path = f"{TEST_DIR}/filler_{filler_index:03d}.bin"
            filler_files.append(filler_path)

            # Create filler file by appending prmDb.dat multiple times
            # Each append adds ~1KB, do it multiple times to make larger files
            fprime_test_api.clear_histories()
            fprime_test_api.send_command(
                f"{file_manager}.AppendFile",
                ["/prmDb.dat", filler_path],
            )

            time.sleep(0.5)

            # Check if append succeeded or failed
            try:
                fprime_test_api.assert_event(
                    f"{file_manager}.AppendFileSucceeded",
                    timeout=5,
                )
                consecutive_failures = 0
                filler_index += 1

                # Make files larger by appending more
                for _ in range(50):  # Append 50 more times to make ~50KB files
                    fprime_test_api.clear_histories()
                    fprime_test_api.send_command(
                        f"{file_manager}.AppendFile",
                        ["/prmDb.dat", filler_path],
                    )
                    time.sleep(0.1)
                    try:
                        fprime_test_api.assert_event(
                            f"{file_manager}.AppendFileSucceeded",
                            timeout=3,
                        )
                    except AssertionError:
                        # Filesystem getting full
                        print(
                            f"  Created {filler_index} filler files, filesystem nearly full"
                        )
                        consecutive_failures = 3  # Break outer loop
                        break

            except AssertionError:
                consecutive_failures += 1
                print(
                    f"  Filler file {filler_index} failed (attempt {consecutive_failures})"
                )

        print(f"Created {len(filler_files)} filler files to fill filesystem")

        # Now attempt to upload a file when storage is full
        print("\nAttempting file upload to full filesystem...")

        # Generate small local test file for upload attempt
        local_test_file = _generate_test_file(100 * 1024)  # 100KB

        try:
            fprime_test_api.clear_histories()

            # Start upload - expect it to fail
            try:
                fprime_test_api.uplink_file_and_await_completion(
                    local_test_file, dest_path, timeout=30
                )
                print("\nOBSERVATION: Upload SUCCEEDED (unexpected)")
                print("  - Filesystem was not actually full")
            except Exception as e:
                print("\nOBSERVATION: Upload FAILED with exception")
                print(f"  - Error: {type(e).__name__}: {e}")

            # Check for error events
            time.sleep(1)

            # Look for FileWriteError event from FileUplink
            try:
                fprime_test_api.assert_event(
                    f"{file_uplink}.FileWriteError",
                    timeout=3,
                )
                print("  - FileWriteError event was emitted")
            except AssertionError:
                print("  - No FileWriteError event detected")

            # Check if partial file was left behind
            fprime_test_api.clear_histories()
            fprime_test_api.send_command(
                f"{file_manager}.FileSize",
                [dest_path],
            )
            time.sleep(1)

            try:
                event = fprime_test_api.assert_event(
                    f"{file_manager}.FileSizeSucceeded",
                    timeout=3,
                )
                partial_size = event.args[1].val
                print("\nOBSERVATION: Partial file LEFT BEHIND")
                print(f"  - Size: {partial_size} bytes")
                filler_files.append(dest_path)  # Add to cleanup list
            except AssertionError:
                print("\nOBSERVATION: No partial file left behind")

        finally:
            if os.path.exists(local_test_file):
                os.remove(local_test_file)

    finally:
        # CRITICAL: Clean up all filler files to restore clean filesystem
        print("\nCleaning up filler files...")
        _cleanup_large_test_files(fprime_test_api, filler_files)

        # Also try to remove the destination file if it exists
        fprime_test_api.send_and_assert_command(
            f"{file_manager}.RemoveFile",
            [dest_path, True],
            timeout=10,
        )

        # Verify filesystem recovered
        fprime_test_api.clear_histories()
        proves_send_and_assert_command(
            fprime_test_api,
            f"{file_manager}.AppendFile",
            ["/prmDb.dat", f"{TEST_DIR}/recovery_check.txt"],
        )

        try:
            fprime_test_api.assert_event(
                f"{file_manager}.AppendFileSucceeded",
                timeout=10,
            )
            print("\nOBSERVATION: Filesystem RECOVERED after cleanup")
            print("  - Successfully created new file after removing filler files")

            # Clean up recovery check file
            fprime_test_api.send_and_assert_command(
                f"{file_manager}.RemoveFile",
                [f"{TEST_DIR}/recovery_check.txt", True],
                timeout=5,
            )
        except AssertionError:
            print("\nOBSERVATION: Filesystem may still have issues after cleanup")

        print("=" * 60)
