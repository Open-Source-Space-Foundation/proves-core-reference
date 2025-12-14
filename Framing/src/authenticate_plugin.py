"""Authenticate Plugin for Framing Package."""

import hashlib
import hmac
import os
from typing import List, Type

from fprime_gds.common.communication.ccsds.chain import ChainedFramerDeframer
from fprime_gds.common.communication.ccsds.space_data_link import (
    SpaceDataLinkFramerDeframer,
)
from fprime_gds.common.communication.ccsds.space_packet import SpacePacketFramerDeframer
from fprime_gds.common.communication.framing import FramerDeframer
from fprime_gds.plugin.definitions import gds_plugin


def get_default_auth_key_from_header() -> str:
    """
    Read the authentication key from AuthDefaultKey.h file.

    Returns:
        Default authentication key (without 0x prefix) from AuthDefaultKey.h

    Raises:
        FileNotFoundError: If AuthDefaultKey.h file is not found
        ValueError: If AuthDefaultKey.h does not contain a valid key
        IOError: If there is an error reading the file
    """
    path = "FprimeZephyrReference/Components/Authenticate/AuthDefaultKey.h"

    if not os.path.exists(path):
        raise FileNotFoundError(
            f"AuthDefaultKey.h not found at {path}. "
            "Authentication plugin requires AuthDefaultKey.h to be present. "
            "Ensure the file exists or run 'make generate-auth-key' to create it."
        )

    try:
        with open(path, "r") as f:
            for line in f:
                # Look for line like: #define AUTH_DEFAULT_KEY "4916d208d40612daad6edbc7333c4c13"
                if "AUTH_DEFAULT_KEY" in line and '"' in line:
                    # Extract key from between quotes
                    start = line.find('"') + 1
                    end = line.find('"', start)
                    if start > 0 and end > start:
                        key = line[start:end]
                        # Remove 0x prefix if present (shouldn't be, but handle it)
                        if key.startswith("0x") or key.startswith("0X"):
                            key = key[2:]
                        return key
    except (IOError, OSError) as e:
        raise IOError(
            f"Error reading AuthDefaultKey.h from {path}: {e}. "
            "Authentication plugin cannot proceed without a valid AuthDefaultKey.h file."
        ) from e

    # If we get here, file exists but contains no valid key
    raise ValueError(
        f"No valid key found in {path}. "
        'AuthDefaultKey.h must contain a line with: #define AUTH_DEFAULT_KEY "<key>"'
    )


# pragma: no cover
class AuthenticateFramer(FramerDeframer):
    """FramerDeframer that adds authentication headers and trailers to data frames"""

    def __init__(
        self,
        initial_sequence_number=0,
        spi=0,
        window_size=50,
        authentication_type="HMAC",
        authentication_key=None,
        **kwargs,
    ):
        """Constructor

        Args:
            initial_sequence_number: Initial sequence number (default: 0)
            spi: Security Parameter Index (default: 0)
            window_size: Window size for authentication (default: 50)
            authentication_type: Type of authentication (default: "HMAC")
            authentication_key: Authentication key as hex string without 0x prefix (default: reads from spi_dict.txt)
            **kwargs: Additional keyword arguments (ignored for now)
        """
        super().__init__()
        # Initialize sequence number from CLI argument or default to 0
        self.bytes_seq_num = initial_sequence_number.to_bytes(
            4, byteorder="big", signed=False
        )
        # Store values from CLI arguments or defaults
        self.spi = spi
        self.sequence_number = initial_sequence_number
        self.window_size = window_size
        self.authentication_type = authentication_type
        # Use provided key or read from AuthDefaultKey.h
        if authentication_key is None:
            authentication_key = get_default_auth_key_from_header()
        print(f"Using authentication key: {authentication_key}")
        self.authentication_key = authentication_key

    def frame(self, data: bytes) -> bytes:
        """Frame data by adding authentication header and trailer"""
        # Authentication Header (16 octets/bytes)
        # right now all default but later, should be able

        # SPI (2 bytes/16 bits, currently 0x0001):
        header = b""
        bytes_spi = self.spi.to_bytes(2, byteorder="big", signed=False)
        header += bytes_spi
        # Sequence Number (32 bits/4 bytes, starts at 0x00000000):
        header += self.bytes_seq_num
        sequence_number = int.from_bytes(
            self.bytes_seq_num, byteorder="big", signed=False
        )
        sequence_number += 1

        self.bytes_seq_num = sequence_number.to_bytes(
            len(self.bytes_seq_num), byteorder="big", signed=False
        )

        data = header + data

        # compute HMAC
        # should be security header + data (data is frame header and data)

        # Security Trailer of 16 octets in length (TM Baseline)
        # the output MAC is 2*128 bits in total length. (32 bytes)
        # Convert hex string to bytes (16 bytes)
        # Keys are stored without 0x prefix, but handle it if present for backward compatibility
        key_hex = self.authentication_key
        if key_hex.startswith("0x") or key_hex.startswith("0X"):
            key_hex = key_hex[2:]
        key = bytes.fromhex(key_hex)

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
        # Get default key from AuthDefaultKey.h for help text
        try:
            default_key = get_default_auth_key_from_header()
        except (FileNotFoundError, ValueError, IOError):
            default_key = "<not found - run 'make generate-auth-key'>"
        return {
            ("--initial-sequence-number",): {
                "type": int,
                "help": "Initial sequence number for authentication (default: 0)",
                "default": 0,
            },
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
                "help": f"Authentication key as hex string without 0x prefix (default: {default_key} from AuthDefaultKey.h)",
                "default": None,  # Will be set to key from AuthDefaultKey.h in __init__
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
