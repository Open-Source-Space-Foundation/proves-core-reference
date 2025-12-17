"""Thread-safe shared state for multi-satellite SCID switching.

This module provides a singleton that manages the active spacecraft ID (SCID)
and the list of allowed SCIDs. Both the framer plugin and the HTTP switcher
plugin use this shared state to coordinate satellite switching.
"""

import threading
from typing import List, Optional


class ScidState:
    """Thread-safe singleton for managing spacecraft ID state.

    This class implements a double-checked locking singleton pattern to ensure
    thread safety. Both the framer and HTTP server access this same instance
    to read/write the active SCID.
    """

    _instance: Optional["ScidState"] = None
    _lock = threading.Lock()

    def __new__(cls) -> "ScidState":
        """Create or return the singleton instance."""
        if cls._instance is None:
            with cls._lock:
                # Double-check after acquiring lock
                if cls._instance is None:
                    instance = super().__new__(cls)
                    instance._active_scid: Optional[int] = None
                    instance._allowed_scids: List[int] = []
                    instance._state_lock = threading.Lock()
                    cls._instance = instance
        return cls._instance

    def set_allowed_scids(self, scids: List[int]) -> None:
        """Set the list of allowed spacecraft IDs.

        If no active SCID is set, the first SCID in the list becomes active.

        Args:
            scids: List of allowed spacecraft IDs (integers)
        """
        with self._state_lock:
            self._allowed_scids = list(scids)
            if self._active_scid is None and scids:
                self._active_scid = scids[0]

    def get_active_scid(self) -> Optional[int]:
        """Get the currently active spacecraft ID.

        Returns:
            The active SCID, or None if not set
        """
        with self._state_lock:
            return self._active_scid

    def set_active_scid(self, scid: int) -> bool:
        """Set the active spacecraft ID.

        The SCID must be in the allowed list to be set as active.

        Args:
            scid: The spacecraft ID to make active

        Returns:
            True if the SCID was set successfully, False if not in allowed list
        """
        with self._state_lock:
            if scid in self._allowed_scids:
                self._active_scid = scid
                return True
            return False

    def get_allowed_scids(self) -> List[int]:
        """Get the list of allowed spacecraft IDs.

        Returns:
            A copy of the allowed SCIDs list
        """
        with self._state_lock:
            return list(self._allowed_scids)

    def get_state(self) -> dict:
        """Get the complete state as a dictionary.

        Returns:
            Dict with 'active' and 'allowed' keys
        """
        with self._state_lock:
            return {
                "active": self._active_scid,
                "allowed": list(self._allowed_scids),
            }

    @classmethod
    def _reset_for_testing(cls) -> None:
        """Reset the singleton instance for testing purposes.

        This method should only be used in tests to ensure a fresh state.
        """
        with cls._lock:
            cls._instance = None
