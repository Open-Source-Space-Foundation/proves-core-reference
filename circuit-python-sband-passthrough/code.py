"""
CircuitPython Feather RP2350 LoRa Radio forwarder

This code will forward any received LoRa packets to the serial console (sys.stdout). It cycles through neo pixel colors
to indicate packet reception.
"""

import os
import time

import board
import digitalio
import usb_cdc  # for passthrough at end of file
from lib.adafruit_mcp230xx.mcp23017 import (
    MCP23017,  # This is Hacky V5a Devel Stuff###
)
from lib.adafruit_tca9548a import TCA9548A  # This is Hacky V5a Devel Stuff###

# from lib.pysquared.Big_Data import AllFaces  ### This is Hacky V5a Devel Stuff###
from lib.pysquared.beacon import Beacon
from lib.pysquared.cdh import CommandDataHandler
from lib.pysquared.config.config import Config
from lib.pysquared.file_validation.manager.file_validation import FileValidationManager
from lib.pysquared.hardware.burnwire.manager.burnwire import BurnwireManager
from lib.pysquared.hardware.busio import _spi_init, initialize_i2c_bus
from lib.pysquared.hardware.digitalio import initialize_pin
from lib.pysquared.hardware.imu.manager.lsm6dsox import LSM6DSOXManager
from lib.pysquared.hardware.light_sensor.manager.veml7700 import VEML7700Manager
from lib.pysquared.hardware.load_switch.manager.loadswitch_manager import (
    LoadSwitchManager,
)
from lib.pysquared.hardware.magnetometer.manager.lis2mdl import LIS2MDLManager
from lib.pysquared.hardware.power_monitor.manager.ina219 import INA219Manager
from lib.pysquared.hardware.radio.manager.rfm9x import RFM9xManager
from lib.pysquared.hardware.radio.manager.sx1280 import SX1280Manager
from lib.pysquared.hardware.radio.packetizer.packet_manager import PacketManager
from lib.pysquared.hardware.temperature_sensor.manager.mcp9808 import MCP9808Manager
from lib.pysquared.logger import Logger
from lib.pysquared.nvm.counter import Counter
from lib.pysquared.protos.power_monitor import PowerMonitorProto
from lib.pysquared.rtc.manager.microcontroller import MicrocontrollerManager
from lib.pysquared.sleep_helper import SleepHelper
from lib.pysquared.watchdog import Watchdog
from version import __version__

rtc = MicrocontrollerManager()


logger: Logger = Logger(
    error_counter=Counter(0),
    colorized=False,
)

logger.info(
    "Booting",
    hardware_version=os.uname().version,  # type: ignore[attr-defined]
    software_version=__version__,
)


def get_temp(sensor):
    """
    Get temperature
    """
    for i in range(1000):
        print(sensor.get_temperature().value)
        time.sleep(0.1)


watchdog = Watchdog(logger, board.WDT_WDI)
watchdog.pet()

logger.debug("Initializing Config")
config: Config = Config("config.json")

mux_reset = initialize_pin(logger, board.MUX_RESET, digitalio.Direction.OUTPUT, False)

# TODO(nateinaction): fix spi init
spi0 = _spi_init(
    logger,
    board.SPI0_SCK,
    board.SPI0_MOSI,
    board.SPI0_MISO,
)

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

i2c1 = initialize_i2c_bus(
    logger,
    board.SCL1,
    board.SDA1,
    100000,
)

i2c0 = initialize_i2c_bus(
    logger,
    board.SCL0,
    board.SDA0,
    100000,
)

sleep_helper = SleepHelper(logger, config, watchdog)

uhf_radio = RFM9xManager(
    logger,
    config.radio,
    spi0,
    initialize_pin(logger, board.SPI0_CS0, digitalio.Direction.OUTPUT, True),
    initialize_pin(logger, board.RF1_RST, digitalio.Direction.OUTPUT, True),
)

magnetometer = LIS2MDLManager(logger, i2c1)

imu = LSM6DSOXManager(logger, i2c1, 0x6B)

uhf_packet_manager = PacketManager(
    logger,
    uhf_radio,
    config.radio.license,
    Counter(2),
    0.2,
)

cdh = CommandDataHandler(logger, config, uhf_packet_manager)

beacon = Beacon(
    logger,
    config.cubesat_name,
    uhf_packet_manager,
    time.monotonic(),
    imu,
    magnetometer,
    uhf_radio,
    sband_radio,
)

## Initialize the MCP23017 GPIO Expander and its pins ##
GPIO_RESET = initialize_pin(
    logger, board.GPIO_EXPANDER_RESET, digitalio.Direction.OUTPUT, True
)
mcp = MCP23017(i2c1)

# This sets up all of the GPIO pins on the MCP23017
FACE4_ENABLE = mcp.get_pin(8)
FACE0_ENABLE = mcp.get_pin(9)
FACE1_ENABLE = mcp.get_pin(10)
FACE2_ENABLE = mcp.get_pin(11)
FACE3_ENABLE = mcp.get_pin(12)
ENAB_RF = mcp.get_pin(13)
VBUS_RESET = mcp.get_pin(14)
SPI0_CS1 = mcp.get_pin(15)
ENABLE_HEATER = mcp.get_pin(0)
PAYLOAD_PWR_ENABLE = mcp.get_pin(1)
Z_GPIO0 = mcp.get_pin(2)
Z_GPIO1 = mcp.get_pin(3)
RF2_IO2 = mcp.get_pin(4)
RF2_IO1 = mcp.get_pin(5)

# This defines the direction of the GPIO pins
FACE4_ENABLE.direction = digitalio.Direction.OUTPUT
FACE0_ENABLE.direction = digitalio.Direction.OUTPUT
FACE1_ENABLE.direction = digitalio.Direction.OUTPUT
FACE2_ENABLE.direction = digitalio.Direction.OUTPUT
FACE3_ENABLE.direction = digitalio.Direction.OUTPUT
ENAB_RF.direction = digitalio.Direction.OUTPUT
VBUS_RESET.direction = digitalio.Direction.OUTPUT
ENABLE_HEATER.direction = digitalio.Direction.OUTPUT
PAYLOAD_PWR_ENABLE.direction = digitalio.Direction.OUTPUT


load_switch_0 = LoadSwitchManager(FACE0_ENABLE, True)  # type: ignore , upstream on mcp TODO
load_switch_1 = LoadSwitchManager(FACE1_ENABLE, True)  # type: ignore , upstream on mcp TODO
load_switch_2 = LoadSwitchManager(FACE2_ENABLE, True)  # type: ignore , upstream on mcp TODO
load_switch_3 = LoadSwitchManager(FACE3_ENABLE, True)  # type: ignore , upstream on mcp TODO
load_switch_4 = LoadSwitchManager(FACE4_ENABLE, True)  # type: ignore , upstream on mcp TODO


# Face Control Helper Functions
def all_faces_off():
    """
    This function turns off all of the faces. Note the load switches are disabled low.
    """
    load_switch_0.disable_load()
    load_switch_1.disable_load()
    load_switch_2.disable_load()
    load_switch_3.disable_load()
    load_switch_4.disable_load()


def all_faces_on():
    """
    This function turns on all of the faces. Note the load switches are enabled high.
    """
    load_switch_0.enable_load()
    load_switch_1.enable_load()
    load_switch_2.enable_load()
    load_switch_3.enable_load()
    load_switch_4.enable_load()


file_validator = FileValidationManager(logger)

## Face Sensor Stuff ##

# This is the TCA9548A I2C Multiplexer
all_faces_on()
mux_reset.value = True
tca = TCA9548A(i2c1, address=int(0x77))


# Light Sensors
light_sensors = []
try:
    sensor = VEML7700Manager(logger, tca[0])
    light_sensors.append(sensor)
except Exception:
    logger.debug("WARNING!!! Light sensor 0 failed to initialize")
    light_sensors.append(None)
try:
    sensor = VEML7700Manager(logger, tca[1])
    light_sensors.append(sensor)
except Exception:
    logger.debug("WARNING!!! Light sensor 1 failed to initialize")
    light_sensors.append(None)
try:
    sensor = VEML7700Manager(logger, tca[2])
    light_sensors.append(sensor)
except Exception:
    logger.debug("WARNING!!! Light sensor 2 failed to initialize")
    light_sensors.append(None)
try:
    sensor = VEML7700Manager(logger, tca[3])
    light_sensors.append(sensor)
except Exception:
    logger.debug("WARNING!!! Light sensor 3 failed to initialize")
    light_sensors.append(None)
try:
    sensor = VEML7700Manager(logger, tca[4])
    light_sensors.append(sensor)
except Exception:
    logger.debug("WARNING!!! Light sensor 4 failed to initialize")
    light_sensors.append(None)


# Onboard Temp Sensors
temp_sensors = []

# Direct I2C sensors
try:
    temp_sensor5 = MCP9808Manager(logger, i2c0, addr=25)  # Antenna Board
except Exception:
    logger.debug("WARNING!!! Temp sensor 5 (Antenna Board) failed")
    temp_sensor5 = None
temp_sensors.append(temp_sensor5)

try:
    temp_sensor6 = MCP9808Manager(logger, i2c1, addr=27)  # Flight Controller Board
except Exception:
    logger.debug("WARNING!!! Temp sensor 6 (Flight Controller Board) failed")
    temp_sensor6 = None
temp_sensors.append(temp_sensor6)

# TCA-connected temp sensors
try:
    sensor = MCP9808Manager(logger, tca[0], addr=27)
    temp_sensors.append(sensor)
except Exception:
    logger.debug("WARNING!!! Temp sensor (TCA[0]) failed")
    temp_sensors.append(None)
try:
    sensor = MCP9808Manager(logger, tca[1], addr=27)
    temp_sensors.append(sensor)
except Exception:
    logger.debug("WARNING!!! Temp sensor 1]) failed")
    temp_sensors.append(None)
try:
    sensor = MCP9808Manager(logger, tca[2], addr=27)
    temp_sensors.append(sensor)
except Exception:
    logger.debug("WARNING!!! Temp sensor 2 failed")
    temp_sensors.append(None)
try:
    sensor = MCP9808Manager(logger, tca[3], addr=27)
    temp_sensors.append(sensor)
except Exception:
    logger.debug("WARNING!!! Temp sensor3 failed")
    temp_sensors.append(None)
try:
    sensor = MCP9808Manager(logger, tca[4], addr=27)
    temp_sensors.append(sensor)
except Exception:
    logger.debug("WARNING!!! Temp sensor 4 failed")
    temp_sensors.append(None)

battery_power_monitor: PowerMonitorProto = INA219Manager(logger, i2c1, 0x40)
solar_power_monitor: PowerMonitorProto = INA219Manager(logger, i2c1, 0x44)

## Init Misc Pins ##
burnwire_heater_enable = initialize_pin(
    logger, board.FIRE_DEPLOY1_A, digitalio.Direction.OUTPUT, False
)
burnwire1_fire = initialize_pin(
    logger, board.FIRE_DEPLOY1_B, digitalio.Direction.OUTPUT, False
)

## Initializing the Burn Wire ##
antenna_deployment = BurnwireManager(
    logger, burnwire_heater_enable, burnwire1_fire, enable_logic=True
)

# END `repl.py` copy

time_start = time.time()
packet_count = 0
last_packet = None

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
