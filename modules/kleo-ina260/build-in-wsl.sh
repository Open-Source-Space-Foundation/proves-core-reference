#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
reference=${KLEO_REFERENCE:-/home/asr/projects/proves-v5c-reference}
cd "$reference"
unset PYTHONPATH PYTHONHOME
source .venv-ina219/bin/activate
export ZEPHYR_BASE="$reference/lib/zephyr-workspace/zephyr"
export ZEPHYR_SDK_INSTALL_DIR=${ZEPHYR_SDK_INSTALL_DIR:-/home/asr/zephyr-sdk-1.0.1}
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
work=$(mktemp -d "$reference/ina260-driver.XXXXXX")
cp -R "$source_dir" "$work/module"
printf '\nDriver workspace: %s\n' "$work"
printf '\n--- Production C core tests with simulated I2C ---\n'
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=undefined -fno-sanitize-recover=all \
  -I "$work/module/drivers" \
  "$work/module/drivers/ina260_core.c" "$work/module/tests/test_core.c" \
  -o "$work/test-core"
"$work/test-core" | tee "$work/test-results.txt"
printf '\n--- V5c sample build (NO FLASH) ---\n'
west build -d "$work/build" -b proves_flight_control_board_v5c/rp2350a/m33 \
  "$work/module/sample" -- -DBOARD_ROOT="$reference" \
  -DZEPHYR_EXTRA_MODULES="$work/module"
printf '\n--- Driver selection ---\n'
grep -E '^CONFIG_(I2C|SENSOR|KLEO_INA260|INA219)=' "$work/build/zephyr/.config"
printf '\nBuild finished: %s\nNo serial access, flash, or reset was performed.\n' "$work"
