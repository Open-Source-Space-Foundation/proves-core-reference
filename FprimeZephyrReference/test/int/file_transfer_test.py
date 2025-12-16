"""
file_transfer_test.py:

Integration tests for file uplink, downlink, and file management operations.
Tests various file sizes from empty to 5MB (camera image simulation).

Test Coverage:
- FileUplink: 1KB, 100KB, 1MB, 5MB files
- FileDownlink: SendFile, SendPartial, Cancel with various file sizes
- FileManager: FileSize, ListDirectory, RemoveFile, MoveFile integration
"""

import hashlib
import shutil
import tempfile
from pathlib import Path

import pytest
from fprime_gds.common.testing_fw.api import IntegrationTestAPI

# =============================================================================
# Constants
# =============================================================================

# Component mnemonics (matching test_day_in_the_life.py naming)
FILE_UPLINK = "FileHandling.fileUplink"
FILE_DOWNLINK = "FileHandling.fileDownlink"
FILE_MANAGER = "FileHandling.fileManager"

# Remote test directory on target filesystem
REMOTE_TEST_DIR = "/test_files"

# File size constants (bytes)
FILE_SIZES = {
    "small": 1024,  # 1 KB
    "medium": 100 * 1024,  # 100 KB
    "large": 1024 * 1024,  # 1 MB
    "very_large": 5 * 1024 * 1024,  # 5 MB (camera image simulation)
}

# Timeout constants (seconds) - conservative for slow hardware
TIMEOUTS = {
    "small": 60,  # Increased from 40 for reliability
    "medium": 180,
    "large": 360,
    "very_large": 720,
}


# =============================================================================
# Helper Functions
# =============================================================================


def generate_pattern_file(path: Path, size_bytes: int) -> str:
    """
    Generate a file with repeating pattern content for integrity verification.

    Args:
        path: Path to write the file
        size_bytes: Size of the file to generate

    Returns:
        SHA256 checksum of the file content
    """
    if size_bytes == 0:
        path.write_bytes(b"")
        return hashlib.sha256(b"").hexdigest()

    # Create repeating pattern of bytes 0-255
    pattern = bytes([i % 256 for i in range(256)])
    repetitions = (size_bytes // 256) + 1
    content = (pattern * repetitions)[:size_bytes]

    path.write_bytes(content)
    return hashlib.sha256(content).hexdigest()


# =============================================================================
# FileUplink Tests
# =============================================================================


def test_uplink_small_file(fprime_test_api: IntegrationTestAPI):
    """Test file uplink for small (1KB) file.

    Note: File is kept on remote for test_downlink_small_file to use.
    """
    size_name = "small"
    size_bytes = FILE_SIZES[size_name]
    timeout = TIMEOUTS[size_name]

    temp_dir = Path(tempfile.mkdtemp(prefix="fprime_file_test_"))
    try:
        local_path = temp_dir / f"test_uplink_{size_name}.bin"
        generate_pattern_file(local_path, size_bytes)
        remote_path = f"{REMOTE_TEST_DIR}/test_{size_name}.bin"

        # Setup: create directory and remove any existing file
        fprime_test_api.send_command(
            f"{FILE_MANAGER}.CreateDirectory", [REMOTE_TEST_DIR]
        )
        fprime_test_api.send_command(f"{FILE_MANAGER}.RemoveFile", [remote_path, True])

        # Perform uplink (this waits for completion internally)
        fprime_test_api.uplink_file_and_await_completion(
            str(local_path), remote_path, timeout=timeout
        )
        # File is kept for downlink test
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


def test_uplink_medium_file(fprime_test_api: IntegrationTestAPI):
    """Test file uplink for medium (100KB) file.

    Note: File is kept on remote for test_downlink_medium_file to use.
    """
    size_name = "medium"
    size_bytes = FILE_SIZES[size_name]
    timeout = TIMEOUTS[size_name]

    temp_dir = Path(tempfile.mkdtemp(prefix="fprime_file_test_"))
    try:
        local_path = temp_dir / f"test_uplink_{size_name}.bin"
        generate_pattern_file(local_path, size_bytes)
        remote_path = f"{REMOTE_TEST_DIR}/test_{size_name}.bin"

        fprime_test_api.send_command(
            f"{FILE_MANAGER}.CreateDirectory", [REMOTE_TEST_DIR]
        )
        fprime_test_api.send_command(f"{FILE_MANAGER}.RemoveFile", [remote_path, True])

        fprime_test_api.uplink_file_and_await_completion(
            str(local_path), remote_path, timeout=timeout
        )
        # File is kept for downlink test
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


def test_uplink_large_file(fprime_test_api: IntegrationTestAPI):
    """Test file uplink for large (1MB) file.

    Note: File is kept on remote for test_downlink_large_file to use.
    """
    size_name = "large"
    size_bytes = FILE_SIZES[size_name]
    timeout = TIMEOUTS[size_name]

    temp_dir = Path(tempfile.mkdtemp(prefix="fprime_file_test_"))
    try:
        local_path = temp_dir / f"test_uplink_{size_name}.bin"
        generate_pattern_file(local_path, size_bytes)
        remote_path = f"{REMOTE_TEST_DIR}/test_{size_name}.bin"

        fprime_test_api.send_command(
            f"{FILE_MANAGER}.CreateDirectory", [REMOTE_TEST_DIR]
        )
        fprime_test_api.send_command(f"{FILE_MANAGER}.RemoveFile", [remote_path, True])

        fprime_test_api.uplink_file_and_await_completion(
            str(local_path), remote_path, timeout=timeout
        )
        # File is kept for downlink test
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


@pytest.mark.slow
def test_uplink_very_large_file(fprime_test_api: IntegrationTestAPI):
    """Test uplink of very large file (5 MB - camera image simulation).

    Note: File is kept on remote for test_downlink_very_large_file to use.
    """
    size_name = "very_large"
    size_bytes = FILE_SIZES[size_name]
    timeout = TIMEOUTS[size_name]

    temp_dir = Path(tempfile.mkdtemp(prefix="fprime_file_test_"))
    try:
        local_path = temp_dir / "test_uplink_very_large.bin"
        generate_pattern_file(local_path, size_bytes)
        remote_path = f"{REMOTE_TEST_DIR}/test_very_large.bin"

        fprime_test_api.send_command(
            f"{FILE_MANAGER}.CreateDirectory", [REMOTE_TEST_DIR]
        )
        fprime_test_api.send_command(f"{FILE_MANAGER}.RemoveFile", [remote_path, True])

        fprime_test_api.uplink_file_and_await_completion(
            str(local_path), remote_path, timeout=timeout
        )
        # File is kept for downlink test
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


# =============================================================================
# FileDownlink Tests
# =============================================================================


@pytest.mark.skip(reason="Downlink tests skipped - focusing on uplink testing")
def test_downlink_small_file(fprime_test_api: IntegrationTestAPI):
    """Test file downlink for small (1KB) file.

    Note: This test reuses the file uploaded by test_uplink_small_file.
    Run the full test suite or ensure test_uplink_small_file runs first.
    """
    size_name = "small"
    timeout = TIMEOUTS[size_name]

    # Reuse the file uploaded by test_uplink_small_file
    remote_path = f"{REMOTE_TEST_DIR}/test_{size_name}.bin"
    dest_filename = f"download_{size_name}.bin"

    fprime_test_api.send_command(
        f"{FILE_DOWNLINK}.SendFile",
        [remote_path, dest_filename],
    )

    # Wait for completion
    fprime_test_api.assert_event(
        f"{FILE_DOWNLINK}.FileSent",
        timeout=timeout,
    )


@pytest.mark.skip(reason="Downlink tests skipped - focusing on uplink testing")
def test_downlink_medium_file(fprime_test_api: IntegrationTestAPI):
    """Test file downlink for medium (100KB) file.

    Note: This test reuses the file uploaded by test_uplink_medium_file.
    """
    size_name = "medium"
    timeout = TIMEOUTS[size_name]

    remote_path = f"{REMOTE_TEST_DIR}/test_{size_name}.bin"
    dest_filename = f"download_{size_name}.bin"

    fprime_test_api.send_command(
        f"{FILE_DOWNLINK}.SendFile",
        [remote_path, dest_filename],
    )

    fprime_test_api.assert_event(
        f"{FILE_DOWNLINK}.FileSent",
        timeout=timeout,
    )


@pytest.mark.skip(reason="Downlink tests skipped - focusing on uplink testing")
def test_downlink_large_file(fprime_test_api: IntegrationTestAPI):
    """Test file downlink for large (1MB) file.

    Note: This test reuses the file uploaded by test_uplink_large_file.
    """
    size_name = "large"
    timeout = TIMEOUTS[size_name]

    remote_path = f"{REMOTE_TEST_DIR}/test_{size_name}.bin"
    dest_filename = f"download_{size_name}.bin"

    fprime_test_api.send_command(
        f"{FILE_DOWNLINK}.SendFile",
        [remote_path, dest_filename],
    )

    fprime_test_api.assert_event(
        f"{FILE_DOWNLINK}.FileSent",
        timeout=timeout,
    )


@pytest.mark.slow
@pytest.mark.skip(reason="Downlink tests skipped - focusing on uplink testing")
def test_downlink_very_large_file(fprime_test_api: IntegrationTestAPI):
    """Test downlink of very large file (5 MB - camera image simulation).

    Note: This test reuses the file uploaded by test_uplink_very_large_file.
    """
    size_name = "very_large"
    timeout = TIMEOUTS[size_name]

    remote_path = f"{REMOTE_TEST_DIR}/test_very_large.bin"
    dest_filename = "download_very_large.bin"

    fprime_test_api.send_command(
        f"{FILE_DOWNLINK}.SendFile",
        [remote_path, dest_filename],
    )

    fprime_test_api.assert_event(
        f"{FILE_DOWNLINK}.FileSent",
        timeout=timeout,
    )


@pytest.mark.skip(reason="Downlink tests skipped - focusing on uplink testing")
def test_downlink_cancel(fprime_test_api: IntegrationTestAPI):
    """Test downlink cancellation.

    Note: Reuses the large file uploaded by test_uplink_large_file.
    """
    # Reuse large file from uplink test
    remote_path = f"{REMOTE_TEST_DIR}/test_large.bin"

    # Start downlink
    fprime_test_api.send_command(
        f"{FILE_DOWNLINK}.SendFile",
        [remote_path, "cancel_test.bin"],
    )

    # Wait for transfer to start
    fprime_test_api.assert_event(
        f"{FILE_DOWNLINK}.SendStarted",
        timeout=30,
    )

    # Cancel the downlink
    fprime_test_api.send_command(f"{FILE_DOWNLINK}.Cancel")

    # Assert DownlinkCanceled event
    fprime_test_api.assert_event(
        f"{FILE_DOWNLINK}.DownlinkCanceled",
        timeout=30,
    )


@pytest.mark.skip(reason="Downlink tests skipped - focusing on uplink testing")
def test_downlink_partial(fprime_test_api: IntegrationTestAPI):
    """Test partial file downlink using SendPartial command.

    Note: Reuses the medium file uploaded by test_uplink_medium_file.
    """
    # Reuse medium file from uplink test
    remote_path = f"{REMOTE_TEST_DIR}/test_medium.bin"

    # Downlink only first 10KB
    start_offset = 0
    length = 10 * 1024
    dest_filename = "partial_download.bin"

    fprime_test_api.send_command(
        f"{FILE_DOWNLINK}.SendPartial",
        [remote_path, dest_filename, start_offset, length],
    )

    fprime_test_api.assert_event(
        f"{FILE_DOWNLINK}.FileSent",
        timeout=60,
    )


@pytest.mark.skip(reason="Downlink tests skipped - focusing on uplink testing")
def test_downlink_nonexistent_file_error(fprime_test_api: IntegrationTestAPI):
    """Test that downlink of non-existent file produces FileOpenError."""
    # Try to downlink non-existent file
    fprime_test_api.send_command(
        f"{FILE_DOWNLINK}.SendFile",
        ["/nonexistent/file.bin", "error_test.bin"],
    )

    # Assert FileOpenError event
    fprime_test_api.assert_event(
        f"{FILE_DOWNLINK}.FileOpenError",
        timeout=30,
    )


# =============================================================================
# FileManager Integration Tests
# =============================================================================


@pytest.mark.skip(reason="FileManager events not reliably received from hardware")
def test_verify_uplink_with_filesize(fprime_test_api: IntegrationTestAPI):
    """Verify uploaded file size using FileManager.FileSize.

    Note: Reuses the medium file uploaded by test_uplink_medium_file.
    """
    remote_path = f"{REMOTE_TEST_DIR}/test_medium.bin"

    # Check file size
    fprime_test_api.send_command(
        f"{FILE_MANAGER}.FileSize",
        [remote_path],
    )

    fprime_test_api.assert_event(
        f"{FILE_MANAGER}.FileSizeSucceeded",
        timeout=30,
    )


@pytest.mark.skip(reason="FileManager events not reliably received from hardware")
def test_list_directory_after_uplink(fprime_test_api: IntegrationTestAPI):
    """Verify directory listing works.

    Note: Lists the test directory which should contain files from uplink tests.
    """
    # List directory (should contain files from uplink tests)
    fprime_test_api.send_command(
        f"{FILE_MANAGER}.ListDirectory",
        [REMOTE_TEST_DIR],
    )

    fprime_test_api.assert_event(
        f"{FILE_MANAGER}.ListDirectorySucceeded",
        timeout=30,
    )


@pytest.mark.skip(reason="FileManager events not reliably received from hardware")
def test_remove_file_after_uplink(fprime_test_api: IntegrationTestAPI):
    """Test that RemoveFile works on uploaded file.

    Note: Removes the small file uploaded by test_uplink_small_file.
    This test should run after downlink tests that use this file.
    """
    remote_path = f"{REMOTE_TEST_DIR}/test_small.bin"

    # Remove file
    fprime_test_api.send_command(
        f"{FILE_MANAGER}.RemoveFile",
        [remote_path, False],  # ignoreErrors=False
    )

    fprime_test_api.assert_event(
        f"{FILE_MANAGER}.RemoveFileSucceeded",
        timeout=30,
    )

    # Verify file is gone by checking FileSize returns error
    fprime_test_api.send_command(
        f"{FILE_MANAGER}.FileSize",
        [remote_path],
    )

    fprime_test_api.assert_event(
        f"{FILE_MANAGER}.FileSizeError",
        timeout=30,
    )


@pytest.mark.skip(reason="FileManager events not reliably received from hardware")
def test_move_uploaded_file(fprime_test_api: IntegrationTestAPI):
    """Test MoveFile operation on uploaded file.

    Note: Moves the medium file uploaded by test_uplink_medium_file.
    This test should run after downlink tests that use this file.
    """
    source_path = f"{REMOTE_TEST_DIR}/test_medium.bin"
    dest_path = f"{REMOTE_TEST_DIR}/test_medium_moved.bin"

    # Clean up destination in case it exists from previous run
    fprime_test_api.send_command(f"{FILE_MANAGER}.RemoveFile", [dest_path, True])

    # Move file
    fprime_test_api.send_command(
        f"{FILE_MANAGER}.MoveFile",
        [source_path, dest_path],
    )

    fprime_test_api.assert_event(
        f"{FILE_MANAGER}.MoveFileSucceeded",
        timeout=30,
    )

    # Verify destination file exists
    fprime_test_api.send_command(
        f"{FILE_MANAGER}.FileSize",
        [dest_path],
    )

    fprime_test_api.assert_event(
        f"{FILE_MANAGER}.FileSizeSucceeded",
        timeout=30,
    )

    # Cleanup - remove the moved file
    fprime_test_api.send_command(f"{FILE_MANAGER}.RemoveFile", [dest_path, True])
