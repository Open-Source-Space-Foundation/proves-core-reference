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
        print("framing data:", data)
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
