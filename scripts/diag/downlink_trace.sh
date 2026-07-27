#!/usr/bin/env bash
# downlink_trace.sh -- v6 of the /keys CI-failure forensics (INVESTIGATION.md).
#
# v5 (hang_thread_walk.*) showed the firmware is healthy and the DOWNLINK is
# dead from boot: ComQueue never leaves its initial WAITING state, so nothing is
# ever framed (TmFramer master frame count stays 0 on both the UART and LoRa
# paths) and the CDC stays silent.  This probe watches the status path that is
# supposed to release ComQueue --
#     comStub.comStatusOut -> framer -> aggregator -> spacePacketFramer
#                          -> comQueue.comStatusIn
# -- live from reset, with a hardware watchpoint on ComQueue.m_state plus
# breakpoints on each forwarding stage, and reports where the status dies.
# Read logic lives in downlink_trace.gdb.
#
# Local bench only (not CI).  Drives the board over a Raspberry Pi Debug Probe
# (CMSIS-DAP SWD).  The two USB CDC ttys involved:
#   BOARD_TTY  /dev/tty.usbmodem1101  - the target's own USB CDC (F'/GDS link)
#   PROBE_TTY  /dev/tty.usbmodem102   - the Debug Probe's UART bridge
# OpenOCD reaches SWD via the probe's CMSIS-DAP USB interface (not a tty).
#
# Read-only apart from the breakpoints/watchpoint it sets, all on a target that
# is reset at the start of the run.  Never writes flash or config.
set -u

BOARD_TTY=${BOARD_TTY:-/dev/tty.usbmodem1101}
PROBE_TTY=${PROBE_TTY:-/dev/tty.usbmodem102}

# OpenOCD: the raspberrypi fork (do NOT substitute a nix/brew openocd -- the RP2350
# support and the CMSIS-DAP build the bench relies on live in this fork).  Local
# bench keeps it under ~/code/...; the CI runner uses ~/openocd.  Override with
# OOCD_HOME.
OOCD_HOME=${OOCD_HOME:-}
if [ -z "$OOCD_HOME" ]; then
  for cand in ~/code/github.com/raspberrypi/openocd ~/openocd; do
    [ -x "$cand/src/openocd" ] && { OOCD_HOME="$cand"; break; }
  done
fi
[ -x "$OOCD_HOME/src/openocd" ] || { echo "downlink_trace: openocd not found (set OOCD_HOME to the raspberrypi openocd checkout)"; exit 1; }
OCD="$OOCD_HOME/src/openocd"
OCD_ARGS=(-s "$OOCD_HOME/tcl"
  -f "$OOCD_HOME/tcl/interface/cmsis-dap.cfg"
  -f "$OOCD_HOME/tcl/target/rp2350.cfg"
  -c "adapter speed 5000")

HERE=$(cd "$(dirname "$0")" && pwd)
GDBCMDS="$HERE/downlink_trace.gdb"
[ -f "$GDBCMDS" ] || { echo "downlink_trace: missing $GDBCMDS"; exit 1; }

# --- symbol ELF (local flight build; load addrs 0x1018_xxxx) ---
ELF=""
for cand in \
  build-fprime-automatic-zephyr/zephyr/zephyr.elf \
  build-artifacts/zephyr/fprime-zephyr-deployment; do
  [ -f "$cand" ] && { ELF="$cand"; break; }
done
[ -z "$ELF" ] && { echo "downlink_trace: symbol ELF not found; run 'make generate build' first."; exit 1; }
echo "downlink_trace: ELF   $ELF"
echo "downlink_trace: board $BOARD_TTY   probe $PROBE_TTY"

# --- ARM gdb ---
GDB=""
for cand in arm-zephyr-eabi-gdb gdb-multiarch arm-none-eabi-gdb; do
  command -v "$cand" >/dev/null 2>&1 && { GDB="$cand"; break; }
done
[ -z "$GDB" ] && GDB=$(ls ~/zephyr-sdk*/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb 2>/dev/null | tail -1)
[ -z "$GDB" ] && { echo "downlink_trace: no arm/multiarch gdb found."; exit 1; }
echo "downlink_trace: gdb   $GDB"

# --- optional: capture the board CDC so telemetry-going-silent is timestamped ---
SERIAL_LOG=${SERIAL_LOG:-board-serial.log}
SNIFF_PID=""
if [ -e "$BOARD_TTY" ]; then
  ( cat "$BOARD_TTY" > "$SERIAL_LOG" 2>/dev/null ) &
  SNIFF_PID=$!
  echo "downlink_trace: sniffing $BOARD_TTY -> $SERIAL_LOG"
else
  echo "downlink_trace: note: $BOARD_TTY not present; skipping serial sniff"
fi

# --- OpenOCD: init + keep the gdb server up; the .gdb file drives reset/run/halt ---
"$OCD" "${OCD_ARGS[@]}" -c "init" -c "echo {downlink_trace: gdb server on :3333}" &
OCD_PID=$!
cleanup() { [ -n "$SNIFF_PID" ] && kill "$SNIFF_PID" 2>/dev/null; kill "$OCD_PID" 2>/dev/null; }
trap cleanup EXIT
sleep 2

# All run/halt sequencing is done with `monitor` inside the command file, so gdb
# never thinks the target is running.  That means gdb would happily serve stale
# cached data across a resume, so disable both memory caches here; the command
# file flushes the register cache after each halt.
GDB_ARGS=(-q -nx -batch "$ELF"
  -ex "set pagination off"
  -ex "set confirm off"
  -ex "set print pretty on"
  -ex "set backtrace past-main on"
  -ex "set stack-cache off"
  -ex "set code-cache off"
  -ex "target extended-remote localhost:3333"
  -x "$GDBCMDS")

TIMEOUT_BIN=$(command -v timeout || command -v gtimeout || true)
if [ -n "$TIMEOUT_BIN" ]; then
  "$TIMEOUT_BIN" 300 "$GDB" "${GDB_ARGS[@]}" \
    || echo "downlink_trace: gdb exited non-zero / timed out"
else
  "$GDB" "${GDB_ARGS[@]}" \
    || echo "downlink_trace: gdb exited non-zero (see output above)"
fi

echo "downlink_trace: done (board serial capture in $SERIAL_LOG,"
echo "                        openocd log in downlink-trace-openocd.log)"
