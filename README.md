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

# How to add a component

Note: This guide has not been confirmed for UART and I2C connections yet, please edit and add clarification as needed

### 1. Update the Board Overlay (TODO Change this if we are using the dtsi instead)


Declare your hardware devices in the Zephyr device tree overlay or .dts file


GPIO examples:
```
 aliases {
        burnwire0 = &burnwire0;
        burnwire1 = &burnwire1;
    };

burnwire0: burnwire0 {
        gpios = <&gpio0 28 GPIO_ACTIVE_HIGH>;
        label = "Burnwire 0, 28";
    };

    ...
```

I2C Example

```
/ {
    aliases {
        myi2c = &i2c1;        // alias for the I²C bus
        mysensor = &sensor0;  // alias for the device on the bus
    };
};

&i2c1 {
    status = "okay";
    mysensor: sensor0@48 {
        compatible = "myvendor,my-sensor";
        reg = <0x48>;   // I²C address of the sensor
    };
};
```
For GPIOs, the alias points to a pin node.

For I²C, you can alias either the bus or a device on the bus.

For UART, the alias points to the UART hardware peripheral.

### 2. Pull the device tree nodes into C++ structs that Zephyr can use. Zephyr drivers require a handle to the hardware node; this step binds your C++ driver to the actual hardware configuration from the overlay.

TODO: Check if the open step needs to happen (can't find clear documentation on it!@@@@@@@@)


In RefereneceDeploymentTopology.cpp, you want to get it from the device tree.

Start by creating a node identifier https://docs.zephyrproject.org/latest/build/dts/api-usage.html#dt-node-identifiers. In this example we use DT_ALIAS because we assume you used ALIAS from step 1, but you can create the node identifier however you want.

#### For GPIO:
https://docs.zephyrproject.org/apidoc/latest/structgpio__dt__spec.html

```
static const struct gpio_dt_spec burnwire0Gpio = GPIO_DT_SPEC_GET(DT_ALIAS(burnwire0), gpios);
```
- GPIO_DT_SPEC_GET → Reads pin number, port, and flags from device tree.


#### For a I2C, you would use a i2c_dt_spec instead. https://docs.zephyrproject.org/apidoc/latest/structi2c__dt__spec.html
```
static const struct i2c_dt_spec sensor =
    I2C_DT_SPEC_GET(DT_ALIAS(mysensor));
```

- I2C_DT_SPEC_GET → Gets I²C bus, address, and speed.

#### For UART:
https://docs.zephyrproject.org/latest/build/dts/howtos.html
```
const struct device *const uart_dev =
    DEVICE_DT_GET(DT_ALIAS(myuart));
```

- DEVICE_DT_GET → Retrieves the UART peripheral.

### 3. Declare each driver in the F´ topology and connect it to your component.

The topology defines how components communicate (ports) and ensures F´ can route commands/events between drivers and components.

#### In Topology.fpp

```
# GPIO drivers
instance gpioBurnwire0
instance gpioBurnwire1


# Connect burnwire to GPIOs
connections burnwire {
    burnwire0.gpioSet -> gpioBurnwire0.gpioWrite
}

# Connect sensor to I²C driver
connections mysensor {
    mySensor.i2cSend -> i2cDriver.i2cWrite
    i2cDriver.i2cRead -> mySensor.i2cReceive
}

# Connect UART to communication component
connections comms {
    comms.tx -> uartDriver.write
    uartDriver.read -> comms.rx
}
```


### 4. Add Component Instances

Assign Base IDs and create instances of each driver/component. Base IDs are used internally by F´ to route commands/events. Every component must have a unique Base ID.

in instances.fpp create an instance of your new pin. Get the base Id by looking at the instructions for the number at the top of file under Base ID Convention of the instances.fpp

```
instance gpioBurnwire0: Zephyr.ZephyrGpioDriver base id 0x10015100
```

### 5. Make a new component in the components folder. Use the command

fprime-util new --component Components/New_Component

### 6. Add the component (not the port) to the instances.fpp and topology.fpp folder

In topology.fpp:

```
instance burnwire
```

In instances.fpp:
```
instance burnwire: Components.Burnwire base id 0x10017000
```
  Get the base Id by looking at the insrtecutions for the number at the top of file under Base ID Convention of the instances.fpp
