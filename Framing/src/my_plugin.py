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
        print("deframing data:", data)
        if len(data) == 0:
            return None, b"", b""
        return data, b"", b""

    # @classmethod
    # def get_name(cls):
    #     """Name of this implementation provided to CLI"""
    #     return "my-plugin"

    # @classmethod
    # def get_arguments(cls):
    #     """Arguments to request from the CLI"""
    #     return {
    #         ("--scid",): {
    #             "type": lambda input_arg: int(input_arg, 0),
    #             "help": "Spacecraft ID",
    #             "default": 0x44,
    #             "required": False,
    #         },
    #         ("--vcid",): {
    #             "type": lambda input_arg: int(input_arg, 0),
    #             "help": "Virtual channel ID",
    #             "default": 1,
    #             "required": False,
    #         },
    #         ("--frame-size",): {
    #             "type": lambda input_arg: int(input_arg, 0),
    #             "help": "Fixed Size of TM Frames",
    #             "default": 1024,
    #             "required": False,
    #         },
    #     }

    # @classmethod
    # def check_arguments(cls, **kwargs):
    #     """ Check arguments from the CLI """
    #     for composite in cls.get_composites():
    #         subset_arguments = cls.get_argument_subset(composite, kwargs)
    #         if hasattr(composite, "check_arguments"):
    #             composite.check_arguments(**subset_arguments)

    # @classmethod
    # @gds_plugin_implementation
    # def register_framing_plugin(cls):
    #     """Register the MyPlugin plugin"""
    #     return cls


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
