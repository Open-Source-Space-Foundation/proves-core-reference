#!/bin/bash
# Build the sweep app for the v5e in all three PL022 modes.
# Usage: build.sh <west-topdir with .west and keys/proves.pem> <out dir>
set -euo pipefail
TOPDIR="${1:?west topdir}"
OUT="${2:-/tmp/b602}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BOARD_ROOT="$(cd "$HERE/../.." && pwd)"
BOARD=proves_flight_control_board_v5e/rp2350a/m33
WEST="$TOPDIR/fprime-venv/bin/west"
cd "$TOPDIR"
"$WEST" build -p -s "$HERE" -b "$BOARD" -d "$OUT/poll" -- -DBOARD_ROOT="$BOARD_ROOT"
"$WEST" build -p -s "$HERE" -b "$BOARD" -d "$OUT/irq"  -- -DBOARD_ROOT="$BOARD_ROOT" -DEXTRA_CONF_FILE="$HERE/irq.conf"
"$WEST" build -p -s "$HERE" -b "$BOARD" -d "$OUT/dma"  -- -DBOARD_ROOT="$BOARD_ROOT" -DEXTRA_CONF_FILE="$HERE/dma.conf" -DEXTRA_DTC_OVERLAY_FILE="$HERE/dma.overlay"
for m in poll irq dma; do ls -l "$OUT/$m/zephyr/zephyr.signed.bin"; done
