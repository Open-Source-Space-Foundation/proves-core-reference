#!/bin/bash
# Dev-bench only (proves-pi500). Flash the given signed image, then run idle and load (300 s each) into <outdir>.
# Usage: run-both.sh <zephyr.signed.bin> <outdir> [duration_s]
set -euo pipefail
IMG="${1:?signed image}"; OUT="${2:?outdir}"; DUR="${3:-300}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
"$HERE/flash.sh" "$IMG" > "$OUT/flash.log" 2>&1
sleep 15
ls /dev/ttyDEVBOARD > /dev/null
"$HERE/run-condition.sh" idle "$DUR" "$OUT" > "$OUT/run-idle.log" 2>&1 || true
"$HERE/run-condition.sh" load "$DUR" "$OUT" 0.5 > "$OUT/run-load.log" 2>&1 || true
echo "### run-both done $(date -Is)" >> "$OUT/run-load.log"
