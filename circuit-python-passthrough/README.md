# CircuitPython Passthrough

This folder holds the code necessary to set up a flight controller board to act as a GDS ground station passthrough. To install to a board, cd into this directory and run:

```
make install {sband|lora} BOARD_MOUNT_POINT={path to board}
```

You can also use the `make circuit-python` command to get the CircuitPython firmware for the board you are using.
