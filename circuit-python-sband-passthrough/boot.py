"""
boot.py

This file helps to use an RP2350 CircuitPython environment as a passthrough for
interacting with the radio and F Prime GDS. The usb_cdc.enable() function creates a
USB serial endpoint that the GDS can connect to to receive all the data that is
streamed out. The function has to be called in boot.py before the USB device is enabled.
"""

import usb_cdc

usb_cdc.enable(console=True, data=True)
