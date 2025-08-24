# Using the ARM Installation of STM32CubeProgrammer

This guide fixes an issue with the ARM installation of the [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html). This installation uses a slightly different path for its bin folder and results in the `stm32CubeProg.sh` script, provided by Arduino Tools, outputting the following error:

```shell
STM32CubeProgrammer not found (STM32_Programmer_CLI).
  Please install it or add '<STM32CubeProgrammer path>/bin' to your PATH environment:
  https://www.st.com/en/development-tools/stm32cubeprog.html
  Aborting!
```

## Instructions
To resolve this issue, please follow these steps:

1. Locate the `stm32CubeProg.sh` script. This file, by default, will be in the following path:

```shell
~/Library/Arduino15/packages/STMicroelectronics/tools/STM32Tools/2.3.0/
```

2. Edit the `stm32CubeProg.sh` script using an ide or text editor

```sh
# Example
nano ~/Library/Arduino15/packages/STMicroelectronics/tools/STM32Tools/2.3.0/stm32CubeProg.sh
```

3. Locate the following block of code in the `stm32CubeProg.sh` script

```sh
if ! command -v $STM32CP_CLI >/dev/null 2>&1; then
    export PATH="/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin":"$PATH"
fi
```

4. Insert the following block of code after the previous code block

```sh
if ! command -v $STM32CP_CLI >/dev/null 2>&1; then
    export PATH="/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin":"$PATH"
fi
```

The script should look like this:

```sh
...
  Darwin*)
    STM32CP_CLI=STM32_Programmer_CLI
    if ! command -v $STM32CP_CLI >/dev/null 2>&1; then
      export PATH="/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin":"$PATH"
    fi
    if ! command -v $STM32CP_CLI >/dev/null 2>&1; then
      export PATH="/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin":"$PATH"
    fi
    if ! command -v $STM32CP_CLI >/dev/null 2>&1; then
      aborting
    fi
...
```

5. Save the file and you are good to go!

# Return to the [STM32 Uploading Guide][stm32]

[stm32]: ../uploading/stm32.md
