"""
CircuitPython Feather RP2350 LoRa Radio forwarder

This code will forward any received LoRa packets to the serial console (sys.stdout). It cycles through neo pixel colors
to indicate packet reception.
"""

import time

import adafruit_rfm9x
import board
import digitalio

# Turn off auto-reload to prevent LoRa module reset
import supervisor
import usb_cdc

supervisor.runtime.autoreload = False

RADIO_PARAMS = {
    "1": (8, 125000, 125000, 2),
    "2": (8, 500000, 125000, 2),
    "3": (7, 125000, 125000, 1),
    "4": (7, 500000, 125000, 1),
    "U": (7, 500000, 125000, 0),  # Uplink only
}


class Lora(object):
    """LoRa Radio class"""

    CS = digitalio.DigitalInOut(board.SPI0_CS0)
    RESET = digitalio.DigitalInOut(board.RF1_RST)

    def __init__(self, freq_mhz: float = 437.4):
        """Initialize the LoRa module"""
        self.rfm95 = adafruit_rfm9x.RFM9x(board.SPI(), self.CS, self.RESET, freq_mhz)
        self.rfm95.coding_rate = 5
        self.rfm95.preamble_length = 8
        self.mode = "U"
        self.up_count = 0
        self.dw_count = 0

    def transmit(self, data: bytes) -> None:
        """Transmit data over LoRa"""
        if data is None or len(data) == 0:
            return
        _, up_bandwidth, down_bandwidth, _ = RADIO_PARAMS[self._mode]
        self.rfm95.signal_bandwidth = up_bandwidth
        success = self.rfm95.send(data)
        if not success:
            print("[ERROR] Failed to transmit packet")
        self.up_count += 1
        self.rfm95.signal_bandwidth = down_bandwidth

    def receive(self) -> bytes:
        """Receive data over LoRa"""
        if self._mode == "U":
            return b""
        _, _, dw_bandwidth, timeout = RADIO_PARAMS[self._mode]
        self.rfm95.signal_bandwidth = dw_bandwidth
        received = self.rfm95.receive(timeout=timeout)
        if received is not None:
            self.dw_count += 1
            return received
        return b""

    def dump(self):
        """Dump radio parameters"""
        _, up_bandwidth, dw_bandwidth, _ = RADIO_PARAMS[self._mode]
        print(f"[INFO] Uplink: {self.up_count}, Downlink: {self.dw_count}")
        print(
            f"[INFO] Spreading factor: {self.rfm95.spreading_factor}, bandwidth up: {up_bandwidth}, bandwidth down: {dw_bandwidth}"
        )

    @property
    def mode(self) -> str:
        """Set the Lora mode"""
        return self._mode

    @mode.setter
    def mode(self, value: str) -> None:
        """Set the Lora mode"""
        if value not in RADIO_PARAMS:
            raise ValueError(f"{value} is not valid Radio setting")
        self._mode = value
        spreading, _, _, _ = RADIO_PARAMS[value]
        self.rfm95.spreading_factor = spreading


usb_cdc.console.timeout = 0.01
usb_cdc.data.timeout = 0.01
data_console = b""
print("[INFO] LoRa Receiver receiving packets")

lora = Lora()
last_time = time.time()

while True:
    if time.time() - last_time > 5:
        last_time = time.time()
        lora.dump()

    # Read data from USB CDC and transmit over LoRa
    try:
        packet_data = usb_cdc.data.read(usb_cdc.data.in_waiting)
        if packet_data is not None and len(packet_data) > 0:
            lora.transmit(packet_data)
            continue  # Most important to prioritize transmitting over receiving
    except Exception as e:
        print(f"[ERROR] {e} transmitting packet of size {len(packet_data)}")
    # Read data from LoRa and write to USB CDC
    packet = usb_cdc.data.write(lora.receive())

    # Read data from console to change radio parameters
    if usb_cdc.console.in_waiting > 0:
        data_console += usb_cdc.console.read(usb_cdc.console.in_waiting)
    else:
        continue
    try:
        index = data_console.strip().decode("utf-8")
        data_console = b""
        if index == "":
            continue
        lora.mode = index
        lora.dump()
    except (ValueError, TypeError) as exc:
        print(f"[ERROR] {exc}")
