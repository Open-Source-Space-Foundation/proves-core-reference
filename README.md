# Proves Core Reference Project

This is a reference software implementation for the [Proves Kit](https://docs.proveskit.space/en/latest/).

## System Requirements
- F Prime System Requirements listed [here](https://fprime.jpl.nasa.gov/latest/docs/getting-started/installing-fprime/#system-requirements)
- Zephyr dependencies listed [here](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies) (NOTE: Only complete the install dependencies step, as we run through the rest of the steps in this readme.)

## Installation

First, clone the Proves Core Reference repository.

```shell
git clone https://github.com/Open-Source-Space-Foundation/proves-core-reference
```

Next, navigate to the `proves-core-reference` directory and run `make` to set up the project.

```shell
cd proves-core-reference
make
```

## Running the code

Run generate from the `proves-core-reference` directory. This generates the build cache for FPrime. You only need to do generate if something in the core FPrime package has changed
```shell
make generate
```

Then, and every time you change code, run

```shell
make build
```

### Find the path to your board

Next, plug in your board! If you have previously installed a firmware on your board you may not see it show up as a drive. In that case you'll want to put the board into boot loader mode. Then you'll be able to find the location of the board on your computer. It should be called something like RP2350 but you want to find the path to it

For Mac:
```shell
ls -lah /Volumes
```

For Windows:
Check the letter said to be the mount (ex /d/) and then the name of the removable drive (ex /d/RP2350)

For Linux:
```shell
findmnt
```

Now you want to install the firmware to the board.
```shell
cp build-artifacts/zephyr.uf2 [path-to-your-board]
```

Finally, run the fprime-gds.
```shell
make gds
```

## Running Integration Tests

First, start GDS with:
```sh
make gds
```

Then, in another terminal, run the following command to execute the integration tests:
```sh
make test-integration
```

## Running The Radio With CircuitPython

To test the radio setup easily, you can use CircuitPython code on one board and fprime-zephyr on another. This provides a simple client/server setup and lets you observe what data is being sent through the radio.


On the board you want to receive data, make sure you have CircuitPython installed. Follow [these instructions](https://proveskit.github.io/pysquared/getting-started/). You can install the flight software or ground station code from that tutorial as these have the libraries you need, or simply install the CircuitPython firmware and manually add the required libraries.


Once you have CircuitPython running, upload the files from the ```circuit-python-lora-passthrough``` folder in this repo. Make sure to overwrite any existing ```boot.py``` and ```code.py``` files on your CircuitPython board with the ones from this folder.

```boot.py``` enables both virtual serial ports that the device presents over USB. This allows you to use one for the console and one for data. ```code.py``` acts as a LoRa radio forwarder over USB serial: The console port is used for logging and debugging, and is the first serial port that appears when the board is connected. The data port is used for actual data transfer, and is the second serial port.

1. Open the console port on your computer. This is the first serial port that opens when you plug in the circuitpython board. It should start by printing:

```
[INFO] LoRa Receiver receiving packets
[INFO] Packets received: 0
```

Once you have the board running the proves-core-reference radio code (make sure its plugged in!), you should start receiving packets and seeing this on the serial port

2.

Now you want to be able to send commands through the radio. To do this, connect the gds to the circuitpython data port. Run the fprime-gds with the --uart-device parameter set to the serial port that is the second serial port that shows up when you plug in your circuitpython board

Depending on the comdelay, the gds should turn green every time a packet is sent. If you want to change this parameter use

```ReferenceDeployment.comDelay.DIVIDER_PRM_SET``` on the gds. You can set it down to 2, but setting it to 1 may cause issues.
