import hashlib
import hmac
from typing import List, Type

from fprime_gds.common.communication.ccsds.chain import ChainedFramerDeframer
from fprime_gds.common.communication.ccsds.space_data_link import (
    SpaceDataLinkFramerDeframer,
)
from fprime_gds.common.communication.ccsds.space_packet import SpacePacketFramerDeframer
from fprime_gds.common.communication.framing import FramerDeframer
from fprime_gds.plugin.definitions import gds_plugin


# pragma: no cover
class MyPlugin(FramerDeframer):
    def frame(self, data: bytes) -> bytes:
        # Authentication Header (16 octets/bytes)
        # right now all default but later, should be able
        # to change with get_arguments

        # SPI (2 bytes/16 bits, currently 0x0001):
        header = b""
        bytes_spi = b"\x00\x01"
        header += bytes_spi
        # Sequence Number (32 bits/4 bytes, currently 0x00000000):
        bytes_seq_num = b"\x00\x00\x00\x00"
        header += bytes_seq_num

        data = header + data

        # compute HMAC
        # should be security header + data (data is frame header and data)

        # key is also currently hardcoded but should be able to be chosen based on spi later
        # Security Trailer of 16 octets in length (TM Baseline)
        # the output MAC is 128 bits in total length. (16 bytes)
        key = b"65b32a18e0c63a347b56e8ae6c51358a"

        hmac_object = hmac.new(key, data, hashlib.sha256)

        hmac_bytes = hmac_object.digest()

        data += hmac_bytes[:16]  # take first 16 bytes (128 bits)

        return data

    def deframe(self, data: bytes, no_copy=False) -> tuple[bytes, bytes, bytes]:
        if len(data) == 0:
            return None, b"", b""
        return data, b"", b""


@gds_plugin(FramerDeframer)
class AuthenticateCompFramer(ChainedFramerDeframer):
    """Space Data Link Protocol framing and deframing that has a data unit of Space Packets as the central"""

    @classmethod
    def get_composites(cls) -> List[Type[FramerDeframer]]:
        """Return the composite list of this chain
        Innermost FramerDeframer should be first in the list."""
        return [SpacePacketFramerDeframer, MyPlugin, SpaceDataLinkFramerDeframer]

    @classmethod
    def get_name(cls):
        """Name of this implementation provided to CLI"""
        return "authenticate-space-data-link"
