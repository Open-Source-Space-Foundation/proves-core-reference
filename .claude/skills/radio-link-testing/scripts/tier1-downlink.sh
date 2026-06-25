#!/bin/bash
# Tier-1 segmented downlink with ground CRC verification.
#   tier1-downlink.sh <flight-src-file> <num-segments> [max-rounds]
#   DL_REF=<path>  local ground-truth reference file (required).
#
# Downloads <flight-src-file> from the flight FS in N equal byte-range segments
# using SendPartial. For each segment: verifies the received bytes against the
# local reference slice CRC, re-downloads mismatches (up to max-rounds). After
# all segments pass, reassembles on the ground and verifies the whole-file CRC.
# Logs per-segment timing and throughput.
#
# Prerequisites:
#   - Bridge GDS running on 21203 (1 comm holder on /dev/cu.usbmodem21203)
#   - console_logger running on 21101 (flight events at /tmp/flight-console-text.log)
#   - Flight seq in sync with bridge GDS seq file
set -u
cd /Users/ossf-1/proves-core-reference

FLIGHT_SRC="${1:?usage: tier1-downlink.sh <flight-src-file> <num-segments> [max-rounds]}"
N="${2:?num-segments}"
MAXR="${3:-8}"
REF="${DL_REF:?DL_REF env var must point to a local ground-truth copy of the flight file}"
[ -f "$REF" ] || { echo "[dl] ERROR: DL_REF not found: $REF"; exit 1; }

LOG=/tmp/flight-console-text.log
DL_STORE=/tmp/ossf-1
DL_DIR="$DL_STORE/fprime-downlink"
PY=./fprime-venv/bin/python3
PYTEST=( ./fprime-venv/bin/pytest -s -o addopts=""
  --dictionary build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json
  --file-uplink-chunk-size 192 )
T=PROVESFlightControllerReference/test/int/bridge_uplink_test.py

now(){ date +%s; }

mkdir -p "$DL_DIR"

# Derive a clean output basename: //tp_asm.bin -> tp_asm
BASE=$( $PY -c "
import os, sys
p = sys.argv[1].lstrip('/')
print(os.path.splitext(os.path.basename(p))[0])
" "$FLIGHT_SRC" )

# --- pre-compute segment boundaries and slice CRCs from ground reference ---
TOTAL=$( $PY -c "import os, sys; print(os.path.getsize(sys.argv[1]))" "$REF" )
SEG_SIZE=$(( TOTAL / N ))

echo "[dl] $FLIGHT_SRC ($TOTAL bytes, ref=$REF) -> $N segments of ~$SEG_SIZE bytes each"

declare -a OFFSET LEN EXP DL_DEST
for i in $(seq 0 $(( N - 1 ))); do
  OFFSET[$i]=$(( i * SEG_SIZE ))
  if [ "$i" -lt $(( N - 1 )) ]; then
    LEN[$i]=$SEG_SIZE
  else
    LEN[$i]=$(( TOTAL - OFFSET[$i] ))
  fi
  EXP[$i]=$( $PY -c "
import zlib, sys
d = open(sys.argv[1], 'rb').read()
start, end = int(sys.argv[2]), int(sys.argv[3])
print('0x%08x' % (zlib.crc32(d[start:end]) ^ 0xFFFFFFFF))
" "$REF" "${OFFSET[$i]}" "$(( OFFSET[$i] + LEN[$i] ))" )
  DL_DEST[$i]="${BASE}_dl_${i}.bin"
  echo "[dl]   seg_$i  offset=${OFFSET[$i]}  len=${LEN[$i]}  expected=${EXP[$i]}  dest=${DL_DEST[$i]}"
done

# --- rig health check ---
echo "[dl] === rig health check ==="
bridge_holders=$( lsof /dev/cu.usbmodem21203 2>/dev/null | grep -vc "^COMMAND" || true )
if [ "$bridge_holders" != "1" ]; then
  echo "[dl] ERROR: bridge port (21203) should have exactly 1 holder, got $bridge_holders"
  echo "[dl]   Fix: pkill -f fprime_gds.executables.comm && bash /tmp/sweep/start_all_gds.sh"
  exit 1
fi
echo "[dl]   bridge comm OK ($bridge_holders holder on /dev/cu.usbmodem21203)"

# --- verify a received segment against ground truth ---
# GDS writes bytes at their absolute source-file offset (SendPartial semantics):
# seg_i file has OFFSET[i] zero bytes prepended, then LEN[i] bytes of real data.
# We CRC the slice [OFFSET[i] : OFFSET[i]+LEN[i]] and require file >= that size.
verify_seg(){ # idx -> 0 ok / 1 mismatch or missing
  local i="$1" path="$DL_DIR/${DL_DEST[$i]}"
  if [ ! -f "$path" ]; then
    echo "[dl]   verify seg_$i MISSING ($path)"; return 1
  fi
  local got_size got_crc min_size
  got_size=$( $PY -c "import os, sys; print(os.path.getsize(sys.argv[1]))" "$path" )
  min_size=$(( OFFSET[$i] + LEN[$i] ))
  got_crc=$( $PY -c "
import zlib, sys
d = open(sys.argv[1], 'rb').read()
start, end = int(sys.argv[2]), int(sys.argv[2]) + int(sys.argv[3])
print('0x%08x' % (zlib.crc32(d[start:end]) ^ 0xFFFFFFFF))
" "$path" "${OFFSET[$i]}" "${LEN[$i]}" )
  if [ "$got_size" -ge "$min_size" ] && [ "$got_crc" = "${EXP[$i]}" ]; then
    echo "[dl]   verify seg_$i OK  size=$got_size (>=min $min_size)  crc=$got_crc"
    return 0
  else
    echo "[dl]   verify seg_$i MISMATCH  size=$got_size/<$min_size  crc=$got_crc/${EXP[$i]}"
    return 1
  fi
}

# --- downlink one segment ---
downlink_seg(){ # idx
  local i="$1" t0 path="$DL_DIR/${DL_DEST[$i]}"
  t0=$(now)
  echo "[dl]   seg_$i: ${FLIGHT_SRC}[${OFFSET[$i]}:$(( OFFSET[$i]+LEN[$i] ))] -> ${DL_DEST[$i]}"
  # Cancel any lingering in-progress downlink from the previous segment
  SPRAY_N=4 "${PYTEST[@]}" "$T::test_cancel_downlink" >/dev/null 2>&1
  sleep 1
  # Remove stale or partial file so await_downlink doesn't see old data as settled
  rm -f "$path"
  # SPRAY_N=1 is mandatory: higher values cause the GRC to TX during the spray,
  # opening the half-duplex RX window so the flight sends START into that window
  # and the GDS discards all subsequent DATA packets as unexpected.
  DL_SRC="$FLIGHT_SRC" DL_DEST="${DL_DEST[$i]}" \
    DL_OFFSET="${OFFSET[$i]}" DL_LENGTH="${LEN[$i]}" \
    SPRAY_N=1 "${PYTEST[@]}" "$T::test_send_partial" >/dev/null 2>&1
  # Poll until the file is stable (stops growing for 3 s) or WATCH_SECS expires
  DL_DEST="${DL_DEST[$i]}" DL_STORAGE="$DL_STORE" WATCH_SECS=180 \
    "${PYTEST[@]}" "$T::test_await_downlink"
  echo "[dl]   seg_$i complete in $(( $(now)-t0 ))s"
}

GRAND0=$(now)

# --- enable flight TX and suppress competing TM ---
echo "[dl] === enabling flight TX ==="
TM_DIVIDER=60 SPRAY_N=8 "${PYTEST[@]}" "$T::test_suppress_tm" >/dev/null 2>&1
CMD="ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET" ARGS="1" REPS=4 GAP=0.2 \
  "${PYTEST[@]}" "$T::test_send_cmd" >/dev/null 2>&1
TX_STATE=ENABLED SPRAY_N=12 "${PYTEST[@]}" "$T::test_spray_disable_tx" >/dev/null 2>&1
echo "[dl]   TX enabled; pausing 10s for first APID-4 TM packet ..."
sleep 10

# --- round loop: download all segments, re-download any that fail CRC ---
declare -a NEED; for i in $(seq 0 $(( N - 1 ))); do NEED[$i]=1; done
for round in $(seq 0 "$MAXR"); do
  todo=()
  for i in $(seq 0 $(( N - 1 ))); do [ "${NEED[$i]}" = 1 ] && todo+=("$i"); done
  [ "${#todo[@]}" -eq 0 ] && break
  echo "[dl] === round $round: ${#todo[@]} segment(s) to (re)download: ${todo[*]} ==="
  # Re-enable TX at start of each round in case flight rebooted (TX defaults to DISABLED)
  TX_STATE=ENABLED SPRAY_N=12 "${PYTEST[@]}" "$T::test_spray_disable_tx" >/dev/null 2>&1
  for i in "${todo[@]}"; do downlink_seg "$i"; done
  for i in "${todo[@]}"; do verify_seg "$i" && NEED[$i]=0; done
done

# --- disable TX ---
echo "[dl] === disabling flight TX ==="
TX_STATE=DISABLED SPRAY_N=12 "${PYTEST[@]}" "$T::test_spray_disable_tx" >/dev/null 2>&1

# --- report remaining failures ---
remaining=0
for i in $(seq 0 $(( N - 1 ))); do [ "${NEED[$i]}" = 1 ] && remaining=$(( remaining + 1 )); done
echo "[dl] segments done; $remaining still bad after $(( MAXR + 1 )) rounds; elapsed=$(( $(now)-GRAND0 ))s"
[ "$remaining" -ne 0 ] && { echo "[dl] ABORT: $remaining segment(s) unverified — inspect above"; exit 1; }

# --- reassemble on the ground ---
# Each seg_i file has OFFSET[i] zero bytes then LEN[i] bytes of real data.
# Extract the real slice from each file and concatenate in order.
OUT="$DL_DIR/${BASE}_reassembled.bin"
echo "[dl] === reassembling into $OUT ==="
tasm=$(now)
rm -f "$OUT"
for i in $(seq 0 $(( N - 1 ))); do
  $PY -c "
import sys
d = open(sys.argv[1], 'rb').read()
start, end = int(sys.argv[2]), int(sys.argv[2]) + int(sys.argv[3])
sys.stdout.buffer.write(d[start:end])
" "$DL_DIR/${DL_DEST[$i]}" "${OFFSET[$i]}" "${LEN[$i]}" >> "$OUT"
done
echo "[dl]   reassembly done in $(( $(now)-tasm ))s"

# --- verify whole reassembled file ---
got_whole=$( $PY -c "
import zlib, sys
d = open(sys.argv[1], 'rb').read()
print('0x%08x' % (zlib.crc32(d) ^ 0xFFFFFFFF))
" "$OUT" )
exp_whole=$( $PY -c "
import zlib, sys
d = open(sys.argv[1], 'rb').read()
print('0x%08x' % (zlib.crc32(d) ^ 0xFFFFFFFF))
" "$REF" )
echo "[dl] FINAL $OUT  crc=$got_whole  expected=$exp_whole  total=$(( $(now)-GRAND0 ))s"
if [ "$got_whole" = "$exp_whole" ]; then
  echo "[dl] *** DOWNLOAD INTACT ***"
else
  echo "[dl] *** FINAL MISMATCH — reassembled file corrupt ***"
  exit 1
fi
