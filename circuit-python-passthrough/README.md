# CircuitPython Passthrough

This folder holds the code necessary to set up a flight controller board to act as a GDS ground station passthrough. To install to a board, cd into this directory and run:

```sh
make install {sband|lora} BOARD_MOUNT_POINT={path to board}
```

Power-cycle the board after the first install. The USB data serial endpoint is
enabled by `boot.py` and is not created by a CircuitPython soft reload.

The LoRa passthrough auto-detects the SX1276 used on boards through v5d and
the SX1262-based E22-400M30S used on v5e.

Download the CircuitPython firmware for a board with:

```sh
make circuit-python BOARD={v5a|v5b|v5c|v5d|v5e}
```

The v5e command currently downloads the compatible v5d CircuitPython image.
The LoRa passthrough accesses the v5e radio GPIOs directly, so it does not
depend on v5e-specific CircuitPython board aliases.
