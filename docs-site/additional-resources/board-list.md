# Install Board Specific Dependencies

In order to upload software to a board, additional steps may need to be taken depending on the board you are using.

## List of Tested Boards
The following boards were able to successfully deploy this reference deployment. The upload guide contain setup and flashing instructions for specific boards.

| Board Name        | Chip              | Toolchain     | Date Tested   | Board Config  | Upload Guide          | Notes                        |
| ----------------- | ----------------- | ------------- | ------------- | ------------- | --------------------- | ---------------------------- |
| teensy41          | ARM Cortex-M7     | teensy41      | 08/01/2025    | supported     | [README][teensy]      |                              |
| NUCLEO-H723ZG     | ARM Cortex-M7     | nucleo_h723zg | 08/01/2025    | supported     | [README][stm32]       | Requires two USB Connections |




<!-- Links -->
[stm32]: ../uploading/stm32/stm32.md
[teensy]: ../uploading/teensy.md
