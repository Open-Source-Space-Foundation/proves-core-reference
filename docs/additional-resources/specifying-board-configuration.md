# Specifying Zephyr Board Configuration
This reference deployent can be used for any board running zephyr so long as the correct zephyr board configuration is provided.

## Update the Zephyr West Configuration File

If your board is supported by Zephyr, the config file in `./lib/zephyr-workspace/.west/` may need to be updated in order to install the correct board configurations. This deployment by default only installs the board configurations for hal_nxp boards.

The following is an example of a configuration for stm32 boards.
```ini
# In fprime-zephyr-reference/lib/zephyr-workspace/.west/config
[zephyr]
base = zephyr

[manifest]
path = zephyr
group-filter = -hal,-debug
project-filter = -.*,+hal_stm32,+cmsis,+cmsis_6
```

More information on West Configuration files can be found [here](https://docs.zephyrproject.org/latest/develop/west/config.html)

> [!TIP]
> If your board is supported by Zephyr and you are unsure how to set up the configuration file, a temporary solution is to remove the config file and running `west update` to install all board configurations.

## Using Custom Board Configurations

<!-- Commented out for now. Not sure if this will confuse users -->
<!-- Using a custom board configuration may require updating the CMakeLists.txt file depending on where the board folder is located. This example appends the board root of a board that in another directory/repository.

```cmake
list(APPEND BOARD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/fprime-stm32h7-zephyr")
```

Make sure to update the path to match the location of your custom board configuration if needed. More information on creating custom board configurations can be found on zephyr's official documentation [here][custom-board-config]. -->
Information on creating custom board configurations can be found on zephyr's official documentation [here][custom-board-config]

## Update the settings.ini File
In order to specify the board to build for, update the `BOARD` option in the `settings.ini` file to the correct board name.


```ini
# In fprime-zephyr-reference/settings.ini
BOARD=nucleo_h723zg # Example for the NUCLEO-H723ZG (existing zephyr support)
BOARD=teensy41 # Example for the Teensy 4.1 (existing zephyr support)
```

A list of supported boards can be found [here](https://docs.zephyrproject.org/latest/boards/index.html#).

## Update prf.conf
Different boards may have different USB PID and VID configurations. Update the following attributes to your board's PID and VID configurations.

```ini
# In fprime-zephyr-reference/prj.conf
CONFIG_USB_DEVICE_VID=<VID>
CONFIG_USB_DEVICE_PID=<PID>
```

# Return to the [Initial Setup Documentation][initial-setup]

<!-- Links -->
[initial-setup]: ../main-content/initial-setup.md
[custom-board-config]: https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html
