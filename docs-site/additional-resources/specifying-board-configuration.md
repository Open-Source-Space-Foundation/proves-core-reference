# Specifying Board Configuration

Three files decide what the build targets.

| File | Sets | Read by |
| ---- | ---- | ------- |
| [`settings.ini`][settings-ini] | `BOARD` and `BOARD_ROOT` | `fprime-util generate` (`make generate`) |
| [`west.yml`][west-yml] | Zephyr version and the modules to download | `west update` (`make zephyr`) |
| [`.west/config`][west-config] | Where the manifest and Zephyr live | `west` |

To build for another PROVES board, you only change `settings.ini`.

## settings.ini

The board is set in `default_cmake_options`:

```ini
default_cmake_options:  FPRIME_ENABLE_FRAMEWORK_UTS=OFF
                        FPRIME_ENABLE_AUTOCODER_UTS=OFF
                        BOARD_ROOT=.
                        BOARD=proves_flight_control_board_v5e/rp2350a/m33
                        FPRIME_CMAKE_QUIET=ON
                        FPRIME_INSTALL_STATIC_LIBRARIES=OFF
```

- `BOARD` is the Zephyr board target, in the form `<board>/<soc>/<cpu>`. Each board's `identifier` in its `.yaml` file gives the exact string. The [Board List][board-list] lists all three.
- `BOARD_ROOT=.` tells Zephyr to search the `boards/` folder at the repo root. That is where `boards/bronco_space/` lives.

## Switch to another variant

1. Edit the `BOARD=` line in `settings.ini`. Keep its indentation. For v5c:

    ```ini
    BOARD=proves_flight_control_board_v5c/rp2350a/m33
    ```

2. Regenerate the build directory.

    ```sh
    # In proves-core-reference
    make generate
    ```

    `make generate` runs `fprime-util generate --force`. You need it because `make build` only generates when `build-fprime-automatic-zephyr` is missing. An existing build directory keeps the old board.

3. Build.

    ```sh
    # In proves-core-reference
    make build
    ```

If you see CMake cache errors after a board change, run `make clean`, then `make generate build`.

!!! warning
    `make clean` runs `git clean -dfX`. It deletes every gitignored file, not only build output. That includes `fprime-venv`, the Zephyr workspace under `lib/zephyr-workspace/`, `keys/`, `*.uf2` files, and `AuthDefaultKey.h`. The next `make generate` creates a new random `AuthDefaultKey.h`. The GDS and the board must then use that new key.

!!! note
    `make build-mcuboot` does not read `settings.ini`. It always builds MCUboot for v5e. See the [Board List][board-list].

!!! note
    [`CMakePresets.json`][cmake-presets] also sets `BOARD`, to `proves_flight_control_board_v5c/rp2350a/m33`. The Makefile does not use it.

## west.yml

`west.yml` is the west manifest. It is trimmed to RP2040 and RP2350 builds so setup downloads less.

It pulls:

- Zephyr `v4.4.2` from `https://github.com/zephyrproject-rtos/zephyr`, into `lib/zephyr-workspace/zephyr`.
- Only these modules from Zephyr's own manifest (`name-allowlist`): `cmsis`, `cmsis_6`, `hal_rpi_pico`, `picolibc`, `mbedtls`, `mcuboot`, `fatfs`, `hal_st`.
- Pinned projects under `lib/zephyr-workspace/`: the modules above, plus `loramac-node` and `tf-psa-crypto`.

The `babblesim` and `optional` groups are excluded (`group-filter`).

`hal_rpi_pico` is required for RP2350. `hal_st` is marked as "Required for certain sensors". The repo has no STM32 board definition.

!!! note
    A board on another SoC family needs its HAL module. Add it the way `hal_st` is added: an entry in `name-allowlist`, and a project with a path under `lib/zephyr-workspace/modules/hal/`. Then run `make zephyr-workspace` to run `west update`.

## .west/config

`.west/config` is at the repo root, so the repo root is the west workspace.

```ini
[manifest]
path = .
file = west.yml

[zephyr]
base = ./lib/zephyr-workspace/zephyr
```

- `[manifest]` points west at `west.yml` in the repo root.
- `[zephyr] base` points at the Zephyr checkout that `west.yml` downloads.

The file is checked in. `make zephyr-config` only runs `west init --local .` when the file is missing. You do not need to change it to switch between PROVES boards.

<!-- Links -->
[board-list]: ./board-list.md
[settings-ini]: https://github.com/Open-Source-Space-Foundation/proves-core-reference/blob/main/settings.ini
[west-yml]: https://github.com/Open-Source-Space-Foundation/proves-core-reference/blob/main/west.yml
[west-config]: https://github.com/Open-Source-Space-Foundation/proves-core-reference/blob/main/.west/config
[cmake-presets]: https://github.com/Open-Source-Space-Foundation/proves-core-reference/blob/main/CMakePresets.json
