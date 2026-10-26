#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
reference=${KLEO_REFERENCE:-/home/asr/projects/proves-v5c-reference}
unset PYTHONPATH PYTHONHOME
source "$reference/.venv-ina219/bin/activate"
export ZEPHYR_BASE="$reference/lib/zephyr-workspace/zephyr"
export ZEPHYR_TOOLCHAIN_VARIANT=host
work=$(mktemp -d "$reference/ina260-sim.XXXXXX")
cp -R "$source_dir" "$work/module"
gcc -std=c11 -Wall -Wextra -Werror -pedantic \
  -fsanitize=undefined -fno-sanitize-recover=all \
  -I "$work/module/drivers" "$work/module/drivers/ina260_core.c" \
  "$work/module/tests/test_core.c" -o "$work/test-core"
"$work/test-core" | tee "$work/core-results.txt"
cd "$reference"
west build -b native_sim/native/64 -d "$work/build" "$work/module/tests/native" \
  -- -DZEPHYR_EXTRA_MODULES="$work/module"
"$work/build/zephyr/zephyr.exe" | tee "$work/native-results.txt"
printf '\nSimulation results: %s\n' "$work"
