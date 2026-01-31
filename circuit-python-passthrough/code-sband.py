"""
CircuitPython S-band Radio Passthrough

This code provides bidirectional passthrough between S-band radio (SX1280) and USB serial.
"""

import time

import board
import digitalio
import usb_cdc
from lib.pysquared.config.config import Config
from lib.pysquared.hardware.busio import _spi_init
from lib.pysquared.hardware.digitalio import initialize_pin
from lib.pysquared.hardware.radio.manager.sx1280 import SX1280Manager
from lib.pysquared.logger import Logger
from lib.pysquared.nvm.counter import Counter

logger: Logger = Logger(
    error_counter=Counter(0),
    colorized=False,
)

logger.debug("Initializing Config")
config: Config = Config("config.json")

spi1 = _spi_init(
    logger,
    board.SPI1_SCK,
    board.SPI1_MOSI,
    board.SPI1_MISO,
)

sband_radio = SX1280Manager(
    logger,
    config.radio,
    spi1,
    initialize_pin(logger, board.SPI1_CS0, digitalio.Direction.OUTPUT, True),
    initialize_pin(logger, board.RF2_RST, digitalio.Direction.OUTPUT, True),
    initialize_pin(logger, board.RF2_IO0, digitalio.Direction.OUTPUT, True),
    2.4,
    initialize_pin(logger, board.RF2_TX_EN, digitalio.Direction.OUTPUT, False),
    initialize_pin(logger, board.RF2_RX_EN, digitalio.Direction.OUTPUT, False),
)

time_start = time.time()
packet_count = 0
last_packet = None

modParam1 = 0x70  # SF = 7
modParam2 = 0x26  # BW = 406.25 KHz
modParam3 = 0x01  # CR = 4/5

sband_radio._radio.set_Modulation_Params(modParam1, modParam2, modParam3)

# rewrite wait_for_irq to prevent blocking on tx (interfering with rx)
sband_radio._radio.wait_for_irq = lambda: time.sleep(0.01)

print("[INFO] LoRa Receiver receiving packets")
while True:
    # Returns the last packet received if there is no new packet
    packet = sband_radio._radio.receive()

    # Only forward `packet` if it is indeed a new packet
    #   - Every packet is unique due to a unique counter which is prepended to
    #     each packet
    #   - The logical packet never spans more than one physical packet as
    #     packet size is hard-coded to 252 bytes on the Fprime board
    if packet != last_packet:
        usb_cdc.data.write(packet)
        packet_count += 1
        last_packet = packet
    time_delta = time.time() - time_start
    if time_delta > 10:
        print(f"[INFO] Packets received: {packet_count}")
        time_start = time.time()
    # `usb_cdc.data` is None until board is hard reset (after reflashing `boot.py`)
    data = usb_cdc.data.read(usb_cdc.data.in_waiting)
    if len(data) > 0:
        print("Sending packet")
        sband_radio._radio.send(data, header=False)
    time.sleep(1)
