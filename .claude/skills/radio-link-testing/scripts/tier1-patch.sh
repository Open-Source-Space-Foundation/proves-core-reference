#!/bin/bash
# Tier-1 part-level OTA patch loop.
#   tier1.sh <source-file> <num-parts> [max-rounds]
# Splits <source> into N parts, uploads each to //tp_<i>.bin, verifies each with
# on-board CalculateCrc (== zlib.crc32 ^ 0xFFFFFFFF), re-uploads only mismatched
# parts, reassembles with AppendFile, and verifies the whole file. Logs timings.
# Verdict source = /tmp/flight-console-text.log (flight USB console).
set -u
cd /Users/ossf-1/proves-core-reference
SRC="${1:?usage: tier1.sh <source> <num-parts> [max-rounds]}"
N="${2:?num-parts}"
MAXR="${3:-3}"
LOG=/tmp/flight-console-text.log
WORK=/tmp/tier1_parts; rm -rf "$WORK"; mkdir -p "$WORK"
PY=./fprime-venv/bin/python3
PYTEST=( ./fprime-venv/bin/pytest -s -o addopts=""
  --dictionary build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json
  --file-uplink-chunk-size 192 )
T=PROVESFlightControllerReference/test/int/bridge_uplink_test.py

fcrc(){ $PY -c "import zlib,sys;print('0x%08x'%(zlib.crc32(open(sys.argv[1],'rb').read())^0xffffffff))" "$1"; }
now(){ date +%s; }

# --- split locally into N parts, record expected CRCs ---
split -d -n "$N" "$SRC" "$WORK/p_" || exit 1
parts=( "$WORK"/p_* )
echo "[tier1] $SRC -> ${#parts[@]} parts ($(wc -c <"$SRC") bytes); whole F'CRC=$(fcrc "$SRC")"
declare -a EXP DEST
for i in "${!parts[@]}"; do EXP[$i]=$(fcrc "${parts[$i]}"); DEST[$i]="//tp_${i}.bin"; done

# --- upload + verify a single part; sets global VERIFY_OK ---
upload_part(){ # idx  -- kick uplink in bg (await never returns in TX-off mode), poll console
  local i="$1" dst="${DEST[$i]}" src="${parts[$i]}" mark t0 pid s ok=0
  cp "$src" "/tmp/tier1_up_${i}.bin"
  mark=$(wc -l <"$LOG"); t0=$(now)
  UPLINK_SRC="/tmp/tier1_up_${i}.bin" UPLINK_DEST="$dst" UPLINK_TIMEOUT=180 \
    "${PYTEST[@]}" "$T::test_uplink_file" >/dev/null 2>&1 &
  pid=$!
  for s in $(seq 1 90); do
    if LC_ALL=C tail -n +"$((mark+1))" "$LOG" | grep -aq "FileReceived : Received file $dst"; then ok=1; break; fi
    sleep 2
  done
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
  if [ "$ok" = 1 ]; then echo "[tier1]   uploaded $dst in $(( $(now)-t0 ))s ($(wc -c <"$src") bytes)"
  else echo "[tier1]   WARN $dst no FileReceived in $(( $(now)-t0 ))s"; fi
}
verify_part(){ # idx -> 0 ok / 1 mismatch
  local i="$1" dst="${DEST[$i]}" mark got base
  base=$(basename "$dst"); mark=$(wc -l <"$LOG")
  CRC_FILE="$dst" SPRAY_N=10 SPRAY_GAP=0.3 "${PYTEST[@]}" "$T::test_calc_crc" >/dev/null 2>&1
  sleep 1
  got=$(LC_ALL=C tail -n +"$((mark+1))" "$LOG" | LC_ALL=C grep -aoE "$base has CRC value 0x[0-9a-fA-F]+" | tail -1 | grep -aoE "0x[0-9a-fA-F]+")
  if [ -n "$got" ] && [ "$((got))" -eq "$((EXP[$i]))" ]; then echo "[tier1]   verify $dst OK ($got)"; return 0
  else echo "[tier1]   verify $dst MISMATCH got=${got:-none} exp=${EXP[$i]}"; return 1; fi
}

GRAND0=$(now)
# --- round 0: upload all, then verify all; subsequent rounds re-upload mismatches ---
declare -a NEED; for i in "${!parts[@]}"; do NEED[$i]=1; done
for round in $(seq 0 "$MAXR"); do
  todo=(); for i in "${!parts[@]}"; do [ "${NEED[$i]}" = 1 ] && todo+=("$i"); done
  [ "${#todo[@]}" -eq 0 ] && break
  echo "[tier1] === round $round: ${#todo[@]} part(s) to (re)upload: ${todo[*]} ==="
  for i in "${todo[@]}"; do upload_part "$i"; done
  for i in "${todo[@]}"; do verify_part "$i" && NEED[$i]=0; done
done
remaining=0; for i in "${!parts[@]}"; do [ "${NEED[$i]}" = 1 ] && remaining=$((remaining+1)); done
echo "[tier1] parts verified; $remaining still bad after $((MAXR+1)) rounds; deliver-time=$(( $(now)-GRAND0 ))s"
[ "$remaining" -ne 0 ] && { echo "[tier1] ABORT: parts still bad"; exit 1; }

# --- reassemble in order: clear target, append each part ---
ASM=//tp_asm.bin
echo "[tier1] === reassembling -> $ASM ==="
RM_FILE="$ASM" SPRAY_N=8 "${PYTEST[@]}" "$T::test_remove_file" >/dev/null 2>&1; sleep 1
tasm=$(now)
for i in "${!parts[@]}"; do
  mark=$(wc -l <"$LOG")
  # AppendFile is NOT idempotent -> send ONCE, confirm AppendFileSucceeded (retry only if not seen)
  for tries in 1 2 3; do
    APPEND_SRC="${DEST[$i]}" APPEND_TGT="$ASM" SPRAY_N=1 "${PYTEST[@]}" "$T::test_append_file" >/dev/null 2>&1
    for s2 in $(seq 1 8); do
      LC_ALL=C tail -n +"$((mark+1))" "$LOG" | grep -aq "AppendFileSucceeded" && break 2
      sleep 2
    done
  done
done
echo "[tier1]   reassembled in $(( $(now)-tasm ))s"

# --- final whole-file verify ---
mark=$(wc -l <"$LOG")
CRC_FILE="$ASM" SPRAY_N=10 "${PYTEST[@]}" "$T::test_calc_crc" >/dev/null 2>&1; sleep 1
got=$(LC_ALL=C tail -n +"$((mark+1))" "$LOG" | LC_ALL=C grep -aoE "tp_asm.bin has CRC value 0x[0-9a-fA-F]+" | tail -1 | grep -aoE "0x[0-9a-fA-F]+")
echo "[tier1] FINAL $ASM crc=$got expected=$(fcrc "$SRC")  total=$(( $(now)-GRAND0 ))s"
[ "${got,,}" = "$(fcrc "$SRC")" ] && echo "[tier1] *** UPDATE INTACT ***" || echo "[tier1] *** FINAL MISMATCH ***"
