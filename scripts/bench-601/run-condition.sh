#!/bin/bash
# Dev-bench only (proves-pi500). One measurement condition on a fresh boot of the dev FC.
# Usage: run-condition.sh <idle|load> <duration_s> <outdir> [cmd_period_s]
# Needs: cwd = bench checkout (dict, AuthDefaultKey.h, fprime-gds.yml); VENV = python venv with fprime-gds 4.3.0.
set -euo pipefail
COND="${1:?idle|load}"; DUR="${2:?seconds}"; OUT="${3:?outdir}"; PERIOD="${4:-0.5}"
VENV="${VENV:-$HOME/pcr-605/fprime-venv}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
if fuser -s /dev/ttyDEVBOARD 2>/dev/null; then echo "ABORT: /dev/ttyDEVBOARD held"; fuser -v /dev/ttyDEVBOARD; exit 2; fi
echo "### $(date -Is) reset board"
"$HERE/reset.sh"
sleep 20
echo "### $(date -Is) start gds"
"$VENV/bin/fprime-gds" --uart-device /dev/ttyDEVBOARD --gui=none --logs "$OUT/gds-logs-$COND" > "$OUT/gds-$COND.out" 2>&1 &
GDS_PID=$!
cleanup() { kill "$GDS_PID" 2>/dev/null || true; sleep 1; pkill -P "$GDS_PID" 2>/dev/null || true; for p in $(pgrep -f "fprime_gds.executables.*fprime-server-in-$USER" 2>/dev/null); do kill "$p" 2>/dev/null || true; done; }
trap cleanup EXIT
sleep 15
echo "### $(date -Is) run $COND for $DUR s"
BENCH_CONDITION="$COND" BENCH_DURATION_S="$DUR" BENCH_OUT="$OUT/$COND.txt" BENCH_CMD_PERIOD_S="$PERIOD" \
  "$VENV/bin/pytest" -q -p no:cacheprovider "$HERE/measure_test.py" --deployment build-artifacts/zephyr/fprime-zephyr-deployment \
  --logs "$OUT/pytest-logs-$COND" 2>&1 | tail -5 | tee "$OUT/pytest-$COND.log"
echo "### $(date -Is) stop gds"
echo "### done: $OUT/$COND.txt"
