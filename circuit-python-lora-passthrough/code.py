"""
CircuitPython Feather RP2350 LoRa Radio forwarder

This code will forward any received LoRa packets to the serial console (sys.stdout). It cycles through neo pixel colors
to indicate packet reception.
"""

import adafruit_rfm9x
import board
import digitalio
import usb_cdc

# Radio constants
RADIO_FREQ_MHZ = 437.4
CS = digitalio.DigitalInOut(board.RFM9X_CS)
RESET = digitalio.DigitalInOut(board.RFM9X_RST)

rfm95 = adafruit_rfm9x.RFM9x(board.SPI(), CS, RESET, RADIO_FREQ_MHZ)
rfm95.spreading_factor = 8
rfm95.signal_bandwidth = 125000
rfm95.coding_rate = 5
rfm95.preamble_length = 8
packet_count = 0
print("[INFO] LoRa Receiver receiving packets")
while True:
    # Look for a new packet - wait up to 2 seconds:
    packet = rfm95.receive(timeout=2.0)
    # If no packet was received during the timeout then None is returned.
    if packet is not None:
        usb_cdc.data.write(packet)
        packet_count += 1
    data = usb_cdc.console.read(usb_cdc.data.in_waiting)
    try:
        rate = int(data)
        if rate < 7 or rate > 9:
            raise ValueError("Spreading factor must be between 7 and 9")
        print(f"[INFO] Setting spreading factor to {rate}")
        rfm95.spreading_factor = rate
    except (ValueError, TypeError):
        pass
    data = usb_cdc.data.read(usb_cdc.data.in_waiting)
    if len(data) > 0:
        rfm95.send(data)
