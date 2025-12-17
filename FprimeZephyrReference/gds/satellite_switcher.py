"""Satellite Switcher HTTP API for F Prime GDS.

This module provides an HTTP server that allows operators to switch between
satellites at runtime. It runs as a GdsFunction plugin alongside the main GDS.

Endpoints:
    GET  /satellites        - List all satellites and current active
    GET  /satellites/active - Get current active satellite
    POST /satellites/<scid> - Switch to specified satellite

Example usage:
    curl http://localhost:8089/satellites
    curl -X POST http://localhost:8089/satellites/0x45
"""

import json
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Optional

from fprime_gds.executables.apps import GdsFunction
from fprime_gds.plugin.definitions import gds_plugin

from .shared_state import ScidState


class SatelliteSwitcherHandler(BaseHTTPRequestHandler):
    """HTTP request handler for satellite switching API."""

    def _send_json_response(self, data: dict, status: int = 200):
        """Send a JSON response.

        Args:
            data: Dictionary to serialize as JSON
            status: HTTP status code
        """
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()
        self.wfile.write(json.dumps(data, indent=2).encode("utf-8"))

    def _format_scid(self, scid: Optional[int]) -> Optional[str]:
        """Format SCID as hex string for JSON response."""
        return f"0x{scid:02X}" if scid is not None else None

    def do_OPTIONS(self):
        """Handle CORS preflight requests."""
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        """Handle GET requests."""
        state = ScidState()

        if self.path == "/satellites" or self.path == "/satellites/":
            # List all satellites
            allowed = state.get_allowed_scids()
            active = state.get_active_scid()
            self._send_json_response(
                {
                    "allowed": [self._format_scid(s) for s in allowed],
                    "active": self._format_scid(active),
                }
            )

        elif self.path == "/satellites/active":
            # Get active satellite only
            active = state.get_active_scid()
            self._send_json_response(
                {
                    "active": self._format_scid(active),
                }
            )

        else:
            self._send_json_response(
                {
                    "error": "Not found",
                    "endpoints": [
                        "GET /satellites",
                        "GET /satellites/active",
                        "POST /satellites/<scid>",
                    ],
                },
                status=404,
            )

    def do_POST(self):
        """Handle POST requests for switching satellites."""
        state = ScidState()

        # Parse SCID from path: /satellites/0x45 or /satellites/69
        if self.path.startswith("/satellites/"):
            scid_str = self.path[len("/satellites/") :]
            try:
                # Support both hex (0x45) and decimal (69) formats
                scid = int(scid_str, 0)

                if state.set_active_scid(scid):
                    self._send_json_response(
                        {
                            "success": True,
                            "active": self._format_scid(scid),
                            "message": f"Switched to spacecraft {self._format_scid(scid)}",
                        }
                    )
                else:
                    allowed = state.get_allowed_scids()
                    self._send_json_response(
                        {
                            "success": False,
                            "error": f"SCID {self._format_scid(scid)} not in allowed list",
                            "allowed": [self._format_scid(s) for s in allowed],
                        },
                        status=400,
                    )

            except ValueError:
                self._send_json_response(
                    {
                        "success": False,
                        "error": f"Invalid SCID format: {scid_str}",
                    },
                    status=400,
                )
        else:
            self._send_json_response(
                {
                    "error": "Use POST /satellites/<scid> to switch satellites",
                },
                status=400,
            )

    def log_message(self, format, *args):
        """Override to customize logging."""
        print(f"[SatelliteSwitcher] {args[0]}")


def _parse_scids(scids_str: str) -> list:
    """Parse comma-separated SCID string into list of integers."""
    if not scids_str:
        return []
    scid_list = []
    for scid in scids_str.split(","):
        scid = scid.strip()
        if scid:
            scid_list.append(int(scid, 0))
    return scid_list


@gds_plugin(GdsFunction)
class SatelliteSwitcher(GdsFunction):
    """GDS plugin that runs an HTTP server for satellite switching.

    This plugin starts an HTTP server on a configurable port (default 8089)
    that provides a REST API for switching between satellites at runtime.
    """

    def __init__(self, switcher_port: int = 8089, scids: str = "", **kwargs):
        """Initialize the satellite switcher.

        Args:
            switcher_port: Port for the HTTP server (default 8089)
            scids: Comma-separated list of allowed spacecraft IDs
            **kwargs: Additional arguments (ignored)
        """
        self.port = switcher_port
        self.scids_str = scids
        self.server: Optional[HTTPServer] = None
        self.server_thread: Optional[threading.Thread] = None

    def run(self, parsed_args):
        """Start the HTTP server in a daemon thread.

        Args:
            parsed_args: Parsed command-line arguments
        """
        # Initialize ScidState with allowed SCIDs
        if self.scids_str:
            scid_list = _parse_scids(self.scids_str)
            if scid_list:
                ScidState().set_allowed_scids(scid_list)
                print(
                    f"[SatelliteSwitcher] Configured SCIDs: {[hex(s) for s in scid_list]}"
                )

        # Create and start the HTTP server
        self.server = HTTPServer(("0.0.0.0", self.port), SatelliteSwitcherHandler)

        self.server_thread = threading.Thread(
            target=self._serve_forever,
            name="SatelliteSwitcher",
            daemon=True,  # Dies when main thread exits
        )
        self.server_thread.start()

        print(
            f"[SatelliteSwitcher] HTTP API running on http://localhost:{self.port}/satellites"
        )

    def _serve_forever(self):
        """Run the HTTP server (called in thread)."""
        try:
            self.server.serve_forever()
        except Exception as e:
            print(f"[SatelliteSwitcher] Server error: {e}")

    @classmethod
    def get_name(cls) -> str:
        """Get the plugin name for CLI."""
        return "satellite-switcher"

    @classmethod
    def get_arguments(cls) -> dict:
        """Get CLI arguments for this plugin."""
        return {
            ("--switcher-port",): {
                "type": int,
                "default": 8089,
                "help": "Port for satellite switcher HTTP API (default: 8089)",
            },
            ("--scids",): {
                "type": str,
                "default": "",
                "help": "Comma-separated list of allowed spacecraft IDs (e.g., '0x44,0x45')",
            },
        }

    @classmethod
    def check_arguments(cls, switcher_port: int = 8089, scids: str = "", **kwargs):
        """Validate CLI arguments."""
        if switcher_port < 1 or switcher_port > 65535:
            raise TypeError(f"Invalid port number: {switcher_port}")
