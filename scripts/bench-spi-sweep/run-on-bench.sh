#!/bin/bash
# Runs on proves-pi500 (dev bench only). Flashes each mode's signed image into
# slot0 over the dev Debug Probe, then captures the console until the app prints DONE.
# Usage: run-on-bench.sh <dir with poll.signed.bin irq.signed.bin dma.signed.bin> <out dir> [modes...]
set -uo pipefail
IMG="${1:?image dir}"; OUT="${2:?out dir}"; shift 2
MODES="${*:-poll irq dma}"
TTY=/dev/ttyDEVBOARD
OCD() { ~/openocd/src/openocd -s ~/openocd/tcl -f interface/cmsis-dap.cfg -c "adapter serial E6647C74034E602F" -f target/rp2350.cfg -c "adapter speed 5000" "$@"; }
mkdir -p "$OUT"

if fuser "$TTY" >/dev/null 2>&1; then echo "ABORT: $TTY held by: $(fuser -v $TTY 2>&1 | tail -n +2)"; exit 2; fi
if pgrep -af openocd | grep -q E6647C74034E602F; then echo "ABORT: another openocd on the dev probe"; exit 2; fi

for m in $MODES; do
  echo "### $(date -Is) flash $m"
  OCD -c init -c "reset halt" -c "flash erase_address 0x10100000 0x100000" \
      -c "program $IMG/$m.signed.bin 0x10100000 verify" -c "reset run" -c shutdown > "$OUT/flash-$m.log" 2>&1 \
      || { echo "flash $m failed"; tail -5 "$OUT/flash-$m.log"; exit 1; }
  for i in $(seq 1 40); do [ -e "$TTY" ] && break; sleep 0.5; done
  [ -e "$TTY" ] || { echo "no $TTY after flash $m"; exit 1; }
  sleep 1
  stty -F "$TTY" 115200 raw -echo -hupcl
  echo "### $(date -Is) capture $m"
  timeout 600 bash -c 'exec 3<"$0"; while IFS= read -r l <&3; do printf "%s\n" "$l"; case "$l" in DONE*|FATAL*) exit 0;; esac; done' "$TTY" > "$OUT/console-$m.log"
  echo "### $(date -Is) $m: $(grep -c ^CSV, "$OUT/console-$m.log") CSV rows, tail: $(tail -1 "$OUT/console-$m.log")"
done
