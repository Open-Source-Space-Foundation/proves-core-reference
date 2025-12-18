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
"""

import time

import pytest
from common import proves_send_and_assert_command
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

file_manager = "FileHandling.fileManager"

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
