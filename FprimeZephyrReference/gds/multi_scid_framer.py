"""Multi-SCID Framer Plugin for F Prime GDS.

This module provides a framer/deframer that supports dynamic spacecraft ID (SCID)
switching at runtime. It extends the standard SpacePacketSpaceDataLinkFramerDeframer
with the ability to change the active SCID without restarting the GDS.
"""

from typing import List, Type

from fprime_gds.common.communication.ccsds.chain import ChainedFramerDeframer
from fprime_gds.common.communication.ccsds.space_data_link import (
    SpaceDataLinkFramerDeframer,
)
from fprime_gds.common.communication.ccsds.space_packet import SpacePacketFramerDeframer
from fprime_gds.common.communication.framing import FramerDeframer
from fprime_gds.plugin.definitions import gds_plugin

from .shared_state import ScidState


class DynamicScidSpaceDataLinkFramerDeframer(SpaceDataLinkFramerDeframer):
    """SpaceDataLinkFramerDeframer with dynamic SCID support.

    This class extends SpaceDataLinkFramerDeframer to read the spacecraft ID
    from the shared ScidState singleton instead of using a fixed value. This
    allows the SCID to be changed at runtime via the HTTP API.
    """

    def __init__(self, scid, vcid, frame_size):
        """Initialize the framer with dynamic SCID support.

        Args:
            scid: Initial spacecraft ID (used as fallback if state not set)
            vcid: Virtual channel ID
            frame_size: Fixed size of TM frames
        """
        # Call parent init - this sets self.scid
        super().__init__(scid, vcid, frame_size)
        # Store the original scid as fallback
        self._base_scid = self.scid

    @property
    def scid(self):
        """Get the current spacecraft ID from shared state.

        Returns the active SCID from ScidState if available, otherwise
        falls back to the base SCID set during initialization.
        """
        active = ScidState().get_active_scid()
        return active if active is not None else self._base_scid

    @scid.setter
    def scid(self, value):
        """Set the base spacecraft ID.

        This is called during parent __init__ to set the initial value.
        We store it as the fallback value.
        """
        self._base_scid = value


@gds_plugin(FramerDeframer)
class MultiScidFramerDeframer(ChainedFramerDeframer):
    """Chained framer/deframer with multi-satellite SCID switching support.

    This framer combines SpacePacketFramerDeframer (inner) with a dynamic
    SCID version of SpaceDataLinkFramerDeframer (outer). The active SCID
    can be changed at runtime via the SatelliteSwitcher HTTP API.

    Usage:
        fprime-gds --framing-selection multi-scid --scids "0x44,0x45,0x46"
    """

    def __init__(self, scids: str, **kwargs):
        """Initialize the multi-SCID framer.

        Args:
            scids: Comma-separated list of allowed spacecraft IDs
                   (e.g., "0x44,0x45,0x46" or "68,69,70")
            **kwargs: Additional arguments passed to parent framers
        """
        # Parse the SCID list and initialize shared state
        scid_list = self._parse_scids(scids)
        ScidState().set_allowed_scids(scid_list)

        # Call parent init which will create the composite framers
        super().__init__(**kwargs)

    @staticmethod
    def _parse_scids(scids_str: str) -> List[int]:
        """Parse comma-separated SCID string into list of integers.

        Args:
            scids_str: Comma-separated SCIDs (e.g., "0x44,0x45" or "68,69")

        Returns:
            List of integer SCIDs
        """
        scid_list = []
        for scid in scids_str.split(","):
            scid = scid.strip()
            if scid:
                # Support both hex (0x44) and decimal (68) formats
                scid_list.append(int(scid, 0))
        return scid_list

    @classmethod
    def get_composites(cls) -> List[Type[FramerDeframer]]:
        """Return the composite framer/deframer classes.

        The order is innermost first:
        1. SpacePacketFramerDeframer - handles CCSDS Space Packets
        2. DynamicScidSpaceDataLinkFramerDeframer - handles TC/TM with dynamic SCID

        Returns:
            List of FramerDeframer classes in framing order
        """
        return [
            SpacePacketFramerDeframer,
            DynamicScidSpaceDataLinkFramerDeframer,
        ]

    @classmethod
    def get_name(cls) -> str:
        """Get the CLI name for this framer.

        Returns:
            The name used with --framing-selection
        """
        return "multi-scid"

    @classmethod
    def get_arguments(cls) -> dict:
        """Get CLI arguments for this framer.

        Returns:
            Dictionary of argument specifications
        """
        # Get arguments from parent (which collects from composites)
        args = super().get_arguments()

        # Add our custom --scids argument
        args[("--scids",)] = {
            "type": str,
            "help": "Comma-separated list of allowed spacecraft IDs (e.g., '0x44,0x45,0x46')",
            "required": True,
        }

        return args

    @classmethod
    def check_arguments(cls, scids: str = None, **kwargs):
        """Validate CLI arguments.

        Args:
            scids: The --scids argument value
            **kwargs: Other arguments passed to parent check
        """
        # Validate scids argument
        if scids is None:
            raise TypeError("--scids argument is required")

        scid_list = cls._parse_scids(scids)
        if not scid_list:
            raise TypeError("--scids must contain at least one spacecraft ID")

        for scid in scid_list:
            if scid < 0:
                raise TypeError(f"Spacecraft ID {scid} is negative")
            if scid > 0x3FF:
                raise TypeError(f"Spacecraft ID {scid} exceeds maximum (0x3FF)")

        # Call parent validation
        super().check_arguments(**kwargs)
