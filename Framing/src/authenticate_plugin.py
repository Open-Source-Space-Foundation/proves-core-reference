"""Authenticate Plugin for Framing Package."""

import fcntl
import hashlib
import hmac
import os
import threading
from typing import List, Type

from fprime_gds.common.communication.ccsds.chain import ChainedFramerDeframer
from fprime_gds.common.communication.ccsds.space_data_link import (
    SpaceDataLinkFramerDeframer,
)
from fprime_gds.common.communication.ccsds.space_packet import SpacePacketFramerDeframer
from fprime_gds.common.communication.framing import FramerDeframer
from fprime_gds.plugin.definitions import gds_plugin

# Get absolute path to sequence number file relative to this file's location
_SEQUENCE_NUMBER_FILENAME = "sequence_number.bin"
_SEQUENCE_NUMBER_DIR = os.path.dirname(os.path.abspath(__file__))
SEQUENCE_NUMBER_FILE = os.path.join(_SEQUENCE_NUMBER_DIR, _SEQUENCE_NUMBER_FILENAME)


# The flight side (Authenticator.cpp parseHexKey) accepts exactly a 128-bit key as 32 hex
# characters. Anything else either blows up later in bytes.fromhex() during framing or produces
# frames the board silently rejects, so both key sources are normalized and checked here instead.
AUTH_KEY_HEX_LENGTH = 32


def normalize_auth_key(key: str, source: str) -> str:
    """Strip any 0x prefix and validate the key is exactly 128 bits of hex.

    Args:
        key: The key as supplied by the operator
        source: Where it came from, for the error message

    Returns:
        The key as 32 hex characters, without 0x prefix

    Raises:
        ValueError: If the key is not exactly 32 hexadecimal characters
    """
    normalized = key.strip()
    if normalized[:2].lower() == "0x":
        normalized = normalized[2:]

    if len(normalized) != AUTH_KEY_HEX_LENGTH:
        raise ValueError(
            f"Authentication key from {source} is {len(normalized)} hex characters; "
            f"expected exactly {AUTH_KEY_HEX_LENGTH} (a 128-bit key)."
        )
    try:
        bytes.fromhex(normalized)
    except ValueError as exc:
        raise ValueError(
            f"Authentication key from {source} is not valid hexadecimal: {exc}"
        ) from exc

    return normalized


def get_auth_key_from_env() -> str:
    """
    Read the authentication key from the PROVES_AUTH_KEY environment variable.

    The key is never compiled into the flight image (see issue #220), so ground
    tooling must be told the key out-of-band: via --authentication-key or this
    environment variable.

    Returns:
        Authentication key as a hex string (without 0x prefix) from PROVES_AUTH_KEY

    Raises:
        ValueError: If PROVES_AUTH_KEY is unset or not a 128-bit hex key
    """
    key = os.environ.get("PROVES_AUTH_KEY")
    if not key:
        raise ValueError(
            "No authentication key available: pass --authentication-key or set "
            "the PROVES_AUTH_KEY environment variable. The key is provisioned "
            "onto the satellite with the PROVISION_KEY command and is never "
            "compiled into the flight image."
        )
    return normalize_auth_key(key, "the PROVES_AUTH_KEY environment variable")


# pragma: no cover
class AuthenticateFramer(FramerDeframer):
    """FramerDeframer that adds authentication headers and trailers to data frames"""

    def __init__(
        self,
        spi=0,
        window_size=50,
        authentication_type="HMAC",
        authentication_key=None,
        **kwargs,
    ):
        """Constructor

        Args:
            spi: Security Parameter Index (default: 0)
            window_size: Window size for authentication (default: 50)
            authentication_type: Type of authentication (default: "HMAC")
            authentication_key: Authentication key as hex string without 0x prefix (default: reads from PROVES_AUTH_KEY env var)
            **kwargs: Additional keyword arguments (ignored for now)
        """
        super().__init__()

        # Guards against races within this process; see get_sequence_number_from_file
        # for why cross-process races also need an OS-level file lock.
        self._frame_lock = threading.Lock()

        # Store values from CLI arguments or defaults
        self.spi = spi
        self.window_size = window_size
        self.authentication_type = authentication_type
        # Use provided key or read from the PROVES_AUTH_KEY environment variable. Either way the
        # key is validated up front, so a malformed key fails at startup with a clear message
        # rather than mid-run inside frame().
        if authentication_key is None:
            authentication_key = get_auth_key_from_env()
        else:
            authentication_key = normalize_auth_key(
                authentication_key, "--authentication-key"
            )
        self.authentication_key = authentication_key

    def get_sequence_number_from_file(self, filename: str, addition: bool) -> int:
        """Read the sequence number from a file
        If the file does not exist, create it with 0
        If addition is True, increment the sequence number and write back to file
        Otherwise just return what is on there

        The GDS runs the file-uplink encoding chain in a separate process
        (CustomDataHandlers) from the one framing commands (comm), so this
        file is the only thing the two agree on -- each process's own
        AuthenticateFramer instance has no other way to learn what sequence
        number the other has already used. flock makes the read-modify-write
        atomic across processes; without it, two frames built by different
        processes at nearly the same time can both read the same pending
        value and emit it as an on-wire duplicate, which the flight side then
        correctly rejects as a replay (TcSecurityDeframer SequenceNumberInvalid).
        """
        with open(filename, "a+") as f:
            fcntl.flock(f, fcntl.LOCK_EX)
            try:
                f.seek(0)
                content = f.read()
                file_number = int(content) if content else 0
                if addition:
                    file_number += 1
                    f.seek(0)
                    f.truncate()
                    f.write(str(file_number))
            finally:
                fcntl.flock(f, fcntl.LOCK_UN)

        return file_number

    def frame(self, data: bytes) -> bytes:
        """Frame data by adding authentication header and trailer"""
        # Authentication Header (16 octets/bytes)
        # right now all default but later, should be able

        # SPI (2 bytes/16 bits, currently 0x0001):
        header = b""
        bytes_spi = self.spi.to_bytes(2, byteorder="big", signed=False)
        header += bytes_spi

        # Sequence Number (32 bits/4 bytes, starts at 0x00000000): always
        # derive this frame's number directly from the shared file rather
        # than an in-memory cache, so this instance can never emit a number
        # another instance (in this process or another) has already used.
        with self._frame_lock:
            sequence_number = self.get_sequence_number_from_file(
                SEQUENCE_NUMBER_FILE, addition=True
            )
        header += sequence_number.to_bytes(4, byteorder="big", signed=False)

        data = header + data

        # compute HMAC
        # should be security header + data (data is frame header and data)

        # Security Trailer of 16 octets in length (TM Baseline)
        # the output MAC is 2*128 bits in total length. (32 bytes)
        # Convert hex string to bytes (16 bytes). The key was normalized and validated in
        # __init__, so this cannot fail here.
        key = bytes.fromhex(self.authentication_key)

        hmac_object = hmac.new(key, data, hashlib.sha256)

        hmac_bytes = hmac_object.digest()

        data += hmac_bytes[:16]  # take first 16 bytes (128 bits)

        return data

    def deframe(self, data: bytes, no_copy=False) -> tuple[bytes, bytes, bytes]:
        """Not implemented - AuthenticateFramer is only for framing/sending"""
        if len(data) == 0:
            return None, b"", b""
        return data, b"", b""

    @classmethod
    def get_arguments(cls) -> dict:
        """Return CLI argument definitions for this plugin"""
        return {
            ("--spi",): {
                "type": int,
                "help": "Security Parameter Index (default: 0)",
                "default": 0,
            },
            ("--window-size",): {
                "type": int,
                "help": "Window size for authentication (default: 50)",
                "default": 50,
            },
            ("--authentication-type",): {
                "type": str,
                "help": "Type of authentication algorithm (default: HMAC)",
                "default": "HMAC",
            },
            ("--authentication-key",): {
                "type": str,
                "help": "Authentication key as hex string without 0x prefix (default: reads from PROVES_AUTH_KEY env var)",
                "default": None,  # Will be set from PROVES_AUTH_KEY in __init__
            },
        }


@gds_plugin(FramerDeframer)
class AuthenticateCompFramer(ChainedFramerDeframer):
    """Space Data Link Protocol framing and deframing that has a data unit of Space Packets as the central"""

    @classmethod
    def get_composites(cls) -> List[Type[FramerDeframer]]:
        """Return the composite list of this chain
        Innermost FramerDeframer should be first in the list."""
        return [
            SpacePacketFramerDeframer,
            AuthenticateFramer,
            SpaceDataLinkFramerDeframer,
        ]

    @classmethod
    def get_name(cls):
        """Name of this implementation provided to CLI"""
        return "authenticate-space-data-link"

    @classmethod
    def get_arguments(cls) -> dict:
        """Return CLI argument definitions for this plugin"""
        # Collect arguments from composites (AuthenticateFramer)
        all_arguments = {}
        for composite in cls.get_composites():
            if hasattr(composite, "get_arguments"):
                composite_args = composite.get_arguments()
                all_arguments.update(composite_args)
        return all_arguments
