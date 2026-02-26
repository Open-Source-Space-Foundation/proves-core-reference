"""
test_file_manager.py:

Integration tests for the FileManager component.

Tests file handling commands:
- CreateDirectory (success and already-exists error)
- RemoveDirectory (success and not-empty error)
- MoveFile
- RemoveFile (success, with ignoreErrors flag, and error)
- AppendFile
- FileSize
- ListDirectory
"""

import time

file_manager = "FileHandling.fileManager"

# Test directory on the device filesystem
TEST_DIR = "/test_files"
TEST_SUBDIR = f"{TEST_DIR}/subdir"


def _cleanup_test_directory(fprime_test_api):
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
    ]
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


def test_00_setup(fprime_test_api):
    """Setup: Clean up any leftover test files from previous runs"""
    _cleanup_test_directory(fprime_test_api)


def test_01_create_and_remove_directory(fprime_test_api):
    """Test creating and removing a directory"""
    # Create directory
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
        timeout=5,
    )
    fprime_test_api.assert_event(
        f"{file_manager}.CreateDirectorySucceeded",
        timeout=5,
    )

    # Remove directory
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.RemoveDirectory",
        [TEST_DIR],
        timeout=5,
    )
    fprime_test_api.assert_event(
        f"{file_manager}.RemoveDirectorySucceeded",
        timeout=5,
    )


def test_02_file_size(fprime_test_api):
    """Test getting the size of a file"""
    # First create the test directory
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
        timeout=5,
    )

    # Use AppendFile to create a file with known content
    # We'll append prmDb.dat to our test file to create it
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/test_file.txt"],
        timeout=10,
    )
    # File I/O operations may take longer on embedded flash storage
    fprime_test_api.assert_event(
        f"{file_manager}.AppendFileSucceeded",
        timeout=10,
    )

    # Get file size
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.FileSize",
        [f"{TEST_DIR}/test_file.txt"],
        timeout=5,
    )
    event = fprime_test_api.assert_event(
        f"{file_manager}.FileSizeSucceeded",
        timeout=5,
    )
    # Verify size is greater than 0
    assert event.args[1].val > 0, "File size should be greater than 0"

    # Cleanup
    _cleanup_test_directory(fprime_test_api)


def test_03_move_file(fprime_test_api):
    """Test moving a file from one location to another"""
    # Create test directory
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
        timeout=5,
    )

    # Create source file by appending from existing file
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/source.txt"],
        timeout=10,
    )

    # Move file
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.MoveFile",
        [f"{TEST_DIR}/source.txt", f"{TEST_DIR}/moved.txt"],
        timeout=5,
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
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.FileSize",
        [f"{TEST_DIR}/moved.txt"],
        timeout=5,
    )
    fprime_test_api.assert_event(
        f"{file_manager}.FileSizeSucceeded",
        timeout=5,
    )

    # Cleanup
    _cleanup_test_directory(fprime_test_api)


def test_04_append_file(fprime_test_api):
    """Test appending one file to another"""
    # Create test directory
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
        timeout=5,
    )

    # Create first file
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/dest.txt"],
        timeout=10,
    )

    # Get initial size
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.FileSize",
        [f"{TEST_DIR}/dest.txt"],
        timeout=5,
    )
    initial_event = fprime_test_api.assert_event(
        f"{file_manager}.FileSizeSucceeded",
        timeout=5,
    )
    initial_size = initial_event.args[1].val

    # Append file to itself (doubles the content)
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/dest.txt"],
        timeout=10,
    )
    # File I/O operations may take longer on embedded flash storage
    fprime_test_api.assert_event(
        f"{file_manager}.AppendFileSucceeded",
        timeout=10,
    )

    # Verify size increased
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.FileSize",
        [f"{TEST_DIR}/dest.txt"],
        timeout=5,
    )
    final_event = fprime_test_api.assert_event(
        f"{file_manager}.FileSizeSucceeded",
        timeout=5,
    )
    final_size = final_event.args[1].val

    assert final_size > initial_size, (
        f"File size should have increased after append: {initial_size} -> {final_size}"
    )

    # Cleanup
    _cleanup_test_directory(fprime_test_api)


def test_05_remove_file(fprime_test_api):
    """Test removing a file"""
    # Create test directory
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
        timeout=5,
    )

    # Create file
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/test_file.txt"],
        timeout=10,
    )

    # Remove file
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.RemoveFile",
        [f"{TEST_DIR}/test_file.txt", False],  # ignoreErrors=False
        timeout=5,
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

    # Cleanup
    _cleanup_test_directory(fprime_test_api)


def test_06_list_directory(fprime_test_api):
    """Test listing directory contents"""
    # Create test directory
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
        timeout=5,
    )

    # Create a file in the directory
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/test_file.txt"],
        timeout=10,
    )

    # List directory
    fprime_test_api.clear_histories()
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.ListDirectory",
        [TEST_DIR],
        timeout=5,
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

    # Cleanup
    _cleanup_test_directory(fprime_test_api)


def test_07_remove_directory_not_empty_fails(fprime_test_api):
    """Test that removing a non-empty directory fails"""
    # Create test directory
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
        timeout=5,
    )

    # Create a file in the directory
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.AppendFile",
        ["/prmDb.dat", f"{TEST_DIR}/test_file.txt"],
        timeout=10,
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

    # Cleanup
    _cleanup_test_directory(fprime_test_api)


def test_08_remove_file_nonexistent_without_ignore_fails(fprime_test_api):
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


def test_09_create_directory_already_exists_fails(fprime_test_api):
    """Test that creating a directory that already exists fails"""
    # Create directory first
    fprime_test_api.send_and_assert_command(
        f"{file_manager}.CreateDirectory",
        [TEST_DIR],
        timeout=5,
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

    # Cleanup
    _cleanup_test_directory(fprime_test_api)
