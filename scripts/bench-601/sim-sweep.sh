#!/bin/bash
# Feed measured bursts into the arena-589 c3 host simulation (option 2, scen/latency) and sweep uspRadio priority.
# Usage: sim-sweep.sh <outfile> RG50_US RG10_US RG1_US MM_US [HOG_BURST_US HOG_PRIO HOG_PERIOD_US]
#   RG*_US / MM_US: CPU burst per cycle of each rate group / modeManager thread, in us (measured peak run lengths).
#   HOG_*: optional extra periodic burst at priority HOG_PRIO (models the measured max preemption at a given level).
# SPI: CHIP_SPI_HZ=3390000 makes the sim's 255 B ReadBuffer cost 617 us (#602: 9.5 SCK/byte at 3.947 MHz).
set -euo pipefail
OUT="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; RG50="${2:?}"; RG10="${3:?}"; RG1="${4:?}"; MM="${5:?}"
HOGB="${6:-0}"; HOGP="${7:-10}"; HOGPER="${8:-1000000}"
SIM="${SIM:-$HOME/GitHut/arena-589/c3/fprime-zephyr/Drv/UspRadio/PROTOTYPE-589-concurrency/arena-c3/build/opt2}"
cd "$SIM"
{
  echo "# latency.bin opt2 CHIP_SPI_HZ=3390000 RG50_US=$RG50 RG10_US=$RG10 RG1_US=$RG1 MM_US=$MM HOG_BURST_US=$HOGB HOG_PRIO=$HOGP HOG_PERIOD_US=$HOGPER COUNT=400"
  for gap in 5000 25000 60000; do
    for prio in ${PRIOS:-11 5 3}; do
      for seed in 1 2 3; do
        line=$(PROF=5 LEN=252 COUNT=400 GAP_US=$gap SEED=$seed RG50_US=$RG50 RG10_US=$RG10 RG1_US=$RG1 MM_US=$MM RADIO_PRIO=$prio \
               HOG_BURST_US=$HOGB HOG_PRIO=$HOGP HOG_PERIOD_US=$HOGPER CHIP_SPI_HZ=3390000 ./latency.bin 2>/dev/null | grep RESULT)
        echo "gap=$gap prio=$prio seed=$seed | $line"
      done
    done
  done
} | tee "$OUT"
