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

Next, plug in your board! If you have previously installed a firmware on your board you may not see it show up as a drive. In that case you'll want to find it's `tty` port.

To do this, run the following command
```shell
make list-tty
```

Otherwise, you want to find the location of the board on your computer. It should be called something like RP2350 but you want to find the path to it

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
make install BOARD_DIR=[path-to-your-board]
```

Finally, run the fprime-gds.
```shell
make gds
```

# How to add a component

1. Overlay


you also want to add aliases

 aliases {
        burnwire0 = &burnwire0;
        burnwire1 = &burnwire1;
    };


2.

In RefereneceDeploymentTopology.cpp
static const struct gpio_dt_spec burnwireGpio = GPIO_DT_SPEC_GET(DT_ALIAS(burnwire0), gpios);

    gpioBurnwire0.open(burnwire0Gpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);
    gpioBurnwire1.open(burnwire1Gpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);


and

gpioDriver.open(burnwire0Gpio, Zephyr::ZephyrGpioDriver::GpioConfiguration::OUT);

in topology.fpp

connections burnwire1 {
      burnwire1.gpioSet -> gpioDriver.gpioWrite
    }




in instances.fpp
instance gpioBurnwire0: Zephyr.ZephyrGpioDriver base id 0x10015100

  instance gpioBurnwire1: Zephyr.ZephyrGpioDriver base id 0x10015200


in topology.fpp

  instance gpioBurnwire0
    instance gpioBurnwire1

3. Make a new component in the components folder
4. Add the component to the instances and topology folder

in topology.fpp also     instance burnwire
