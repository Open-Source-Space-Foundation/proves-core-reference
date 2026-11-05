"""CircuitPython LoRa radio passthrough for PROVES flight controller boards.

The passthrough supports the SX1276 used through v5d and the SX1262-based
E22-400M30S used on v5e.
"""

import time

import adafruit_rfm9x
import board
import digitalio
import microcontroller

# Turn off auto-reload to prevent LoRa module reset
import supervisor
import usb_cdc
from proves_sx126._sx126x import ERR_NONE, ERR_RX_TIMEOUT, SX126X_REG_RX_GAIN
from proves_sx126.sx1262 import SX1262

supervisor.runtime.autoreload = False

RADIO_PARAMS = {
    "1": (8, 125000, 125000, 2),
    "2": (8, 500000, 125000, 2),
    "3": (7, 125000, 125000, 1),
    "4": (7, 500000, 125000, 1),
    "U": (7, 500000, 125000, 0),  # Uplink only
}

FREQUENCY_MHZ = 437.4
CODING_RATE = 5
PREAMBLE_LENGTH = 8
FPRIME_LORA_HEADER = b"\x00\x00\x00\x00"
SX1262_MAX_POWER_DBM = 22
SX1262_RX_GAIN_BOOSTED = 0x96
SX1262_TCXO_STARTUP_DELAY_US = 10000


class CircuitPythonIRQ:
    """Adapt CircuitPython's DIO value property to the SX126 driver's API."""

    def __init__(self, digital_input: digitalio.DigitalInOut):
        self._digital_input = digital_input

    def value(self) -> bool:
        """Return the current DIO input value."""
        return self._digital_input.value


class CircuitPythonSX1262(SX1262):
    """SX1262 compatibility wrapper for CircuitPython DigitalInOut."""

    def setBlockingCallback(self, blocking: bool, callback=None) -> int:
        """Configure blocking mode without recreating an allocated DIO1 pin."""
        if not blocking:
            return super().setBlockingCallback(blocking, callback)

        self.blocking = True
        self._callbackFunction = self._dummyFunction
        return self.standby()


class SX1276Radio:
    """SX1276 backend used by PROVES flight controller boards through v5d."""

    def __init__(self, freq_mhz: float):
        chip_select = digitalio.DigitalInOut(microcontroller.pin.GPIO9)
        reset = digitalio.DigitalInOut(microcontroller.pin.GPIO6)

        try:
            self._radio = adafruit_rfm9x.RFM9x(
                board.SPI(), chip_select, reset, freq_mhz
            )
        except Exception:
            chip_select.deinit()
            reset.deinit()
            raise

        self._radio.coding_rate = CODING_RATE
        self._radio.preamble_length = PREAMBLE_LENGTH

    @property
    def spreading_factor(self) -> int:
        """Return the configured spreading factor."""
        return self._radio.spreading_factor

    @spreading_factor.setter
    def spreading_factor(self, value: int) -> None:
        self._radio.spreading_factor = value

    @property
    def signal_bandwidth(self) -> int:
        """Return the configured signal bandwidth in Hz."""
        return self._radio.signal_bandwidth

    @signal_bandwidth.setter
    def signal_bandwidth(self, value: int) -> None:
        self._radio.signal_bandwidth = value

    def send(self, data: bytes) -> bool:
        """Transmit a packet."""
        return self._radio.send(data)

    def receive(self, timeout: float) -> bytes | None:
        """Receive a packet."""
        return self._radio.receive(timeout=timeout)

    def idle(self) -> None:
        """Put the radio in standby."""
        self._radio.idle()


class SX1262Radio:
    """SX1262/E22-400M30S backend used by the v5e board."""

    def __init__(self, freq_mhz: float):
        # v5e device tree: SPI1 SCK/MOSI/MISO = GPIO 10/11/12.
        self._spi = board.SPI()
        self._chip_select = self._output_pin(microcontroller.pin.GPIO9, True)
        self._reset = self._output_pin(microcontroller.pin.GPIO6, True)
        self._busy = self._input_pin(microcontroller.pin.GPIO13)
        self._dio1 = self._input_pin(microcontroller.pin.GPIO14)
        self._tx_enable = self._output_pin(microcontroller.pin.GPIO21, False)
        self._rx_enable = self._output_pin(microcontroller.pin.GPIO22, False)

        self._radio = CircuitPythonSX1262(
            self._spi,
            self._chip_select,
            self._dio1,
            self._reset,
            self._busy,
        )
        self._radio.irq = CircuitPythonIRQ(self._dio1)
        self._spreading_factor = RADIO_PARAMS["1"][0]
        self._signal_bandwidth = RADIO_PARAMS["1"][1]

        status = self._radio.begin(
            freq=freq_mhz,
            bw=self._signal_bandwidth / 1000,
            sf=self._spreading_factor,
            cr=CODING_RATE,
            power=SX1262_MAX_POWER_DBM,
            preambleLength=PREAMBLE_LENGTH,
            crcOn=True,
            implicit=False,
            syncWord=0x12,
            tcxoVoltage=1.8,
            blocking=True,
        )
        self._check_status(status, "initialize")
        self._check_status(
            self._radio.setTCXO(1.8, delay=SX1262_TCXO_STARTUP_DELAY_US),
            "configure TCXO startup delay",
        )
        self._check_status(
            self._radio.setDio2AsRfSwitch(False), "disable DIO2 RF switching"
        )
        self._check_status(
            self._radio.writeRegister(SX126X_REG_RX_GAIN, [SX1262_RX_GAIN_BOOSTED], 1),
            "enable boosted RX gain",
        )

    @staticmethod
    def _input_pin(pin: microcontroller.Pin) -> digitalio.DigitalInOut:
        digital_pin = digitalio.DigitalInOut(pin)
        digital_pin.switch_to_input()
        return digital_pin

    @staticmethod
    def _output_pin(pin: microcontroller.Pin, value: bool) -> digitalio.DigitalInOut:
        digital_pin = digitalio.DigitalInOut(pin)
        digital_pin.switch_to_output(value=value)
        return digital_pin

    def _check_status(self, status: int, operation: str) -> None:
        if status != ERR_NONE:
            error = self._radio.STATUS.get(status, str(status))
            raise RuntimeError(f"SX1262 failed to {operation}: {error}")

    def _set_rf_path(self, transmit: bool = False, receive: bool = False) -> None:
        # Never enable both paths at once. The DTS marks both enables active-high.
        self._tx_enable.value = False
        self._rx_enable.value = False
        self._tx_enable.value = transmit
        self._rx_enable.value = receive

    @property
    def spreading_factor(self) -> int:
        """Return the configured spreading factor."""
        return self._spreading_factor

    @spreading_factor.setter
    def spreading_factor(self, value: int) -> None:
        self._check_status(self._radio.setSpreadingFactor(value), "set SF")
        self._spreading_factor = value

    @property
    def signal_bandwidth(self) -> int:
        """Return the configured signal bandwidth in Hz."""
        return self._signal_bandwidth

    @signal_bandwidth.setter
    def signal_bandwidth(self, value: int) -> None:
        self._check_status(self._radio.setBandwidth(value / 1000), "set bandwidth")
        self._signal_bandwidth = value

    def send(self, data: bytes) -> bool:
        """Transmit a packet using the external v5e TX RF path."""
        self._set_rf_path(transmit=True)
        try:
            # F Prime expects the four-byte header that adafruit_rfm9x adds
            # automatically for the legacy SX1276 passthrough.
            _, status = self._radio.send(FPRIME_LORA_HEADER + data)
            if status != ERR_NONE:
                error = self._radio.STATUS.get(status, str(status))
                print(f"[ERROR] SX1262 transmit failed: {error}")
                return False
            return True
        finally:
            self._set_rf_path()

    def receive(self, timeout: float) -> bytes | None:
        """Receive a packet using the external v5e RX RF path."""
        self._set_rf_path(receive=True)
        try:
            received, status = self._radio.recv(
                timeout_en=True, timeout_ms=int(timeout * 1000)
            )
        finally:
            self._set_rf_path()

        if status == ERR_NONE:
            if len(received) < len(FPRIME_LORA_HEADER):
                print("[ERROR] SX1262 received a packet shorter than the LoRa header")
                return None
            # Match adafruit_rfm9x.receive(), which strips its RadioHead
            # header before returning bytes to the USB/GDS transport.
            return received[len(FPRIME_LORA_HEADER) :]
        if status != ERR_RX_TIMEOUT:
            error = self._radio.STATUS.get(status, str(status))
            print(f"[ERROR] SX1262 receive failed: {error}")
        return None

    def idle(self) -> None:
        """Disable both external RF paths and put the radio in standby."""
        self._set_rf_path()
        self._check_status(self._radio.standby(), "enter standby")


class Lora(object):
    """LoRa Radio class"""

    def __init__(self, freq_mhz: float = FREQUENCY_MHZ):
        """Initialize the LoRa module"""
        board_id = getattr(board, "board_id", "").lower()
        if board_id.endswith("v5e"):
            self.radio = SX1262Radio(freq_mhz)
            radio_name = "SX1262"
        else:
            try:
                self.radio = SX1276Radio(freq_mhz)
                radio_name = "SX1276"
            except RuntimeError:
                # A v5d CircuitPython build can be used on v5e because the
                # passthrough references the v5e GPIOs directly.
                self.radio = SX1262Radio(freq_mhz)
                radio_name = "SX1262"

        print(f"[INFO] Initialized {radio_name} LoRa radio")
        self.mode = "1"
        self.up_count = 0
        self.dw_count = 0

    def transmit(self, data: bytes) -> None:
        """Transmit data over LoRa"""
        if data is None or len(data) == 0:
            return
        _, up_bandwidth, down_bandwidth, _ = RADIO_PARAMS[self._mode]
        self.radio.signal_bandwidth = up_bandwidth
        success = self.radio.send(data)
        if not success:
            print("[ERROR] Failed to transmit packet")
        else:
            self.up_count += 1
        self.radio.signal_bandwidth = down_bandwidth

    def receive(self) -> bytes:
        """Receive data over LoRa"""
        if self._mode == "U":
            return b""
        _, _, dw_bandwidth, timeout = RADIO_PARAMS[self._mode]
        self.radio.signal_bandwidth = dw_bandwidth
        received = self.radio.receive(timeout=timeout)
        if received is not None:
            self.dw_count += 1
            return received
        return b""

    def dump(self):
        """Dump radio parameters"""
        _, up_bandwidth, dw_bandwidth, _ = RADIO_PARAMS[self._mode]
        print(f"[INFO] Uplink: {self.up_count}, Downlink: {self.dw_count}")
        print(
            f"[INFO] Spreading factor: {self.radio.spreading_factor}, bandwidth up: {up_bandwidth}, bandwidth down: {dw_bandwidth}"
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
        self.radio.spreading_factor = spreading
        if value == "U":
            self.radio.idle()


data_console = b""
print("[INFO] LoRa Receiver receiving packets")

lora = Lora()
data_serial = usb_cdc.data
if data_serial is None:
    print(
        "[WARNING] USB data serial is unavailable. Power-cycle the board "
        "to apply boot.py; a soft reload is not sufficient."
    )
last_time = time.time()

while True:
    if time.time() - last_time > 5:
        last_time = time.time()
        lora.dump()

    # Read data from USB CDC and transmit over LoRa
    packet_data = b""
    if data_serial is not None:
        try:
            if data_serial.in_waiting > 0:
                packet_data = data_serial.read(data_serial.in_waiting)
            if packet_data:
                lora.transmit(packet_data)
                continue  # Most important to prioritize transmitting over receiving
        except Exception as e:
            print(f"[ERROR] {e} transmitting packet of size {len(packet_data)}")

    # Read data from LoRa and write to USB CDC
    packet = lora.receive()
    if packet and data_serial is not None:
        data_serial.write(packet)

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
