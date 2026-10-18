#!/bin/bash
# Dev-bench only. Hardware reset of the dev FC through the dev probe (max-hold counters restart from zero).
set -euo pipefail
~/openocd/src/openocd -s ~/openocd/tcl -f interface/cmsis-dap.cfg -c "adapter serial E6647C74034E602F" -f target/rp2350.cfg -c "adapter speed 5000" \
  -c init -c "reset run" -c shutdown 2>&1 | grep -E "Error|error" || true
