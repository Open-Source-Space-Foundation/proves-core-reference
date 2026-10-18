#!/bin/bash
# Dev-bench only (proves-pi500). Flash a signed slot0 image through the dev probe and reset the board.
# Refuses to run if anything holds the dev FC tty or an OpenOCD is already on the dev probe.
# Usage: flash.sh <zephyr.signed.bin>
set -euo pipefail
IMG="${1:?signed image}"
if fuser -s /dev/ttyDEVBOARD 2>/dev/null; then echo "ABORT: /dev/ttyDEVBOARD is held: $(fuser -v /dev/ttyDEVBOARD 2>&1 | tail -1)"; exit 2; fi
if pgrep -af openocd | grep -q E6647C74034E602F; then echo "ABORT: another openocd on the dev probe"; exit 2; fi
~/openocd/src/openocd -s ~/openocd/tcl -f interface/cmsis-dap.cfg -c "adapter serial E6647C74034E602F" -f target/rp2350.cfg -c "adapter speed 5000" \
  -c init -c "reset halt" -c "flash erase_address 0x10100000 0x100000" -c "program $IMG 0x10100000 verify" -c "reset run" -c shutdown 2>&1 | grep -E "wrote|verified|Error|error|Warn" || true
