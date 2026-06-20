#!/usr/bin/env python3
"""Log a board's USB serial console to a text file, extracting printable runs.

The flight/GRC consoles emit F-prime event text interleaved with binary
(0x44-padded) idle frames. This pulls out the readable >=5-char ASCII runs and
timestamps them, so a plain `grep` of the output file shows events. The raw bytes
are also kept alongside for forensics.

Usage:
    console_logger.py <serial-device> <output-text-log> [baud]

Example:
    console_logger.py /dev/cu.usbmodem21101 /tmp/flight-console.log
    grep -a FileReceived /tmp/flight-console.log     # read the verdict

Run in the background; it loops until killed. NOTE: reflashing the GRC
re-enumerates USB and this process will exit with a read error — restart it.
"""

import re
import sys
import time

import serial  # from the project's fprime-venv

if len(sys.argv) < 3:
    sys.exit(__doc__)

device = sys.argv[1]
text_path = sys.argv[2]
baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200
raw_path = text_path + ".raw"

ser = serial.Serial(device, baud, timeout=0.3)
printable = re.compile(rb"[ -~]{5,}")

with open(raw_path, "ab") as raw, open(text_path, "a", buffering=1) as txt:
    txt.write(f"\n==== attach {device} {time.strftime('%H:%M:%S')} ====\n")
    while True:
        data = ser.read(4096)
        if not data:
            continue
        raw.write(data)
        raw.flush()
        for match in printable.findall(bytes(data)):
            txt.write(
                f"[{time.strftime('%H:%M:%S')}] {match.decode(errors='replace')}\n"
            )
