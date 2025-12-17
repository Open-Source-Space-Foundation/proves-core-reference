"""Multi-satellite GDS plugins for FprimeZephyrReference.

This package provides GDS plugins for switching between multiple satellites
at runtime without restarting the GDS.

Plugins:
    MultiScidFramerDeframer - Framer with dynamic SCID switching
    SatelliteSwitcher - HTTP API for changing active satellite

Usage:
    Set the environment variable before running fprime-gds:

    export FPRIME_GDS_EXTRA_PLUGINS="FprimeZephyrReference.gds:MultiScidFramerDeframer;FprimeZephyrReference.gds:SatelliteSwitcher"

    Then configure in fprime-gds.yml:

    command-line-options:
      framing-selection: multi-scid
      scids: "0x44,0x45,0x46"
      switcher-port: 8089
"""

from .multi_scid_framer import (
    DynamicScidSpaceDataLinkFramerDeframer,
    MultiScidFramerDeframer,
)
from .satellite_switcher import SatelliteSwitcher
from .shared_state import ScidState

__all__ = [
    "ScidState",
    "MultiScidFramerDeframer",
    "DynamicScidSpaceDataLinkFramerDeframer",
    "SatelliteSwitcher",
]
