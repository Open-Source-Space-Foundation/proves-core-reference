from fprime_gds.common.communication.framing import FramerDeframer
from fprime_gds.plugin.definitions import gds_plugin_implementation


# pragma: no cover
class MyPlugin(FramerDeframer):
    def frame(self, data: bytes) -> bytes:
        print("framing data:", data)
        return data

    def deframe(self, data: bytes, no_copy=False) -> tuple[bytes, bytes, bytes]:
        print("deframing data:", data)
        return data, b"", b""

    @classmethod
    def get_name(cls):
        """Name of this implementation provided to CLI"""
        return "my-plugin"

    @classmethod
    def get_arguments(cls):
        """Arguments to request from the CLI"""
        return {}

    @classmethod
    def check_arguments(cls):
        """Check arguments from the CLI"""
        pass

    @classmethod
    @gds_plugin_implementation
    def register_framing_plugin(cls):
        """Register the MyPlugin plugin"""
        return cls
