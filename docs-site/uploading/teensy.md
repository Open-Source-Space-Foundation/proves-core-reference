# Uploading Guide for the Teensy

This guide sets up `teensy_loader_cli` to flash code onto the Teensy. Alternatively, the `arduino-cli` can also be used. Instructions to setup the `arduino-cli` can be found [here][arduino-cli-guide]

These steps were tested for the following boards:

- Teensy 4.1

## Initial Setup
1. Follow the `teensy_loader_cli` installation guide [here][teensy-loader-cli-guide]
2. If needed, add the `teensy_loader_cli` to the PATH.

> [!NOTE]
> Mac users can install the `teensy_loader_cli` using homebrew.
> To install using homebrew, run the following command:
> ```zsh
> brew install teensy_loader_cli
> ```

## Flashing the Board
```sh
# In fprime-zephyr-reference

# Teensy 4.1
teensy_loader_cli -v -mmcu=TEENSY41 -w build-fprime-automatic-zephyr/zephyr/zephyr.hex
```

[arduino-cli-guide]: https://github.com/fprime-community/fprime-arduino/blob/0d4f023dec007294d073bec64a44d6d8010dce04/docs/arduino-cli-install.md
[teensy-loader-cli-guide]: https://www.pjrc.com/teensy/loader_cli.html
