"""
CircuitPython Feather RP2350 LoRa Radio forwarder

This code will forward any received LoRa packets to the serial console (sys.stdout). It cycles through neo pixel colors
to indicate packet reception.
"""

import time

import adafruit_rfm9x
import board
import digitalio
import usb_cdc

# Radio constants
RADIO_FREQ_MHZ = 437.4
CS = digitalio.DigitalInOut(board.SPI0_CS0)
RESET = digitalio.DigitalInOut(board.RF1_RST)

rfm95 = adafruit_rfm9x.RFM9x(board.SPI(), CS, RESET, RADIO_FREQ_MHZ)
rfm95.spreading_factor = 8
rfm95.signal_bandwidth = 125000
rfm95.coding_rate = 5
rfm95.preamble_length = 8
time_start = time.time()
packet_count = 0
print("[INFO] LoRa Receiver receiving packets")
while True:
    # Look for a new packet - wait up to 2 seconds:
    packet = rfm95.receive(timeout=2.0)
    # If no packet was received during the timeout then None is returned.
    if packet is not None:
        usb_cdc.data.write(packet)
        packet_count += 1
    time_delta = time.time() - time_start
    if time_delta > 10:
        print(f"[INFO] Packets received: {packet_count}")
        time_start = time.time()
    data = usb_cdc.data.read(usb_cdc.data.in_waiting)
    if len(data) > 0:
        rfm95.send(data)
