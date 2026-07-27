#!/usr/bin/env bash
# hang_thread_walk.sh -- v5 of the /keys boot-hang forensics (see INVESTIGATION.md).
#
# v4 (hang_fault_bp.*) proved the hang is NOT a CPU fault: the fault-entry
# breakpoints never fire and a clean halt shows cm0 idle with CFSR=HFSR=0.  So
# there is no faulting frame to catch -- the scheduler simply stops making
# progress.  This probe takes the other approach INVESTIGATION.md calls for:
# reset, free-run into the hang, then halt TWICE a few seconds apart and
#   * compare cycle_count / curr_tick / SysTick to see whether the clock is
#     frozen or still ticking, and
#   * walk _kernel.threads, recovering each swapped-out thread's resume PC/LR
#     from its saved PendSV frame and unwinding it, to see exactly who is
#     blocked and on what.
# Read logic lives in hang_thread_walk.gdb.
#
# Local bench only (not CI).  Drives the board over a Raspberry Pi Debug Probe
# (CMSIS-DAP SWD).  The two USB CDC ttys involved:
#   BOARD_TTY  /dev/tty.usbmodem1101  - the target's own USB CDC (F'/GDS link);
#                                       telemetry goes silent here at the hang
#   PROBE_TTY  /dev/tty.usbmodem102   - the Debug Probe's UART bridge
# OpenOCD reaches SWD via the probe's CMSIS-DAP USB interface (not a tty); the
# ttys are used only to correlate/timestamp the hang.
#
# Read-mostly: it resets, runs, halts and reads state.  The only target writes
# are core-register rewinds used to unwind each blocked thread's stack (restored
# right after, on a target that was reset at the start of the run and is left
# halted at the end).  Never writes flash or config.
set -u

# Free-run windows, in ms.  A = reset -> first halt (long enough to reach the
# failed state), B = gap between the two halts (long enough to span the 30s
# telemetry cadence, so a thread that simply had nothing to do in a short window
# is not mistaken for a wedged one).
WINDOW_A_MS=${WINDOW_A_MS:-12000}
WINDOW_B_MS=${WINDOW_B_MS:-20000}

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
[ -x "$OOCD_HOME/src/openocd" ] || { echo "hang_thread_walk: openocd not found (set OOCD_HOME to the raspberrypi openocd checkout)"; exit 1; }
OCD="$OOCD_HOME/src/openocd"
OCD_ARGS=(-s "$OOCD_HOME/tcl"
  -f "$OOCD_HOME/tcl/interface/cmsis-dap.cfg"
  -f "$OOCD_HOME/tcl/target/rp2350.cfg"
  -c "adapter speed 5000")

HERE=$(cd "$(dirname "$0")" && pwd)
GDBCMDS="$HERE/hang_thread_walk.gdb"
[ -f "$GDBCMDS" ] || { echo "hang_thread_walk: missing $GDBCMDS"; exit 1; }

# --- symbol ELF (local flight build; load addrs 0x1018_xxxx) ---
ELF=""
for cand in \
  build-fprime-automatic-zephyr/zephyr/zephyr.elf \
  build-artifacts/zephyr/fprime-zephyr-deployment; do
  [ -f "$cand" ] && { ELF="$cand"; break; }
done
[ -z "$ELF" ] && { echo "hang_thread_walk: symbol ELF not found; run 'make generate build' first."; exit 1; }
echo "hang_thread_walk: ELF   $ELF"
echo "hang_thread_walk: board $BOARD_TTY   probe $PROBE_TTY"
echo "hang_thread_walk: windows A=${WINDOW_A_MS}ms B=${WINDOW_B_MS}ms"

# --- ARM gdb ---
GDB=""
for cand in arm-zephyr-eabi-gdb gdb-multiarch arm-none-eabi-gdb; do
  command -v "$cand" >/dev/null 2>&1 && { GDB="$cand"; break; }
done
[ -z "$GDB" ] && GDB=$(ls ~/zephyr-sdk*/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb 2>/dev/null | tail -1)
[ -z "$GDB" ] && { echo "hang_thread_walk: no arm/multiarch gdb found."; exit 1; }
echo "hang_thread_walk: gdb   $GDB"

# --- optional: capture the board CDC so telemetry-going-silent is timestamped ---
SERIAL_LOG=${SERIAL_LOG:-board-serial.log}
SNIFF_PID=""
if [ -e "$BOARD_TTY" ]; then
  ( cat "$BOARD_TTY" > "$SERIAL_LOG" 2>/dev/null ) &
  SNIFF_PID=$!
  echo "hang_thread_walk: sniffing $BOARD_TTY -> $SERIAL_LOG"
else
  echo "hang_thread_walk: note: $BOARD_TTY not present; skipping serial sniff"
fi

# --- OpenOCD: init + keep the gdb server up; the .gdb file drives reset/run/halt ---
"$OCD" "${OCD_ARGS[@]}" -c "init" -c "echo {hang_thread_walk: gdb server on :3333}" &
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
  -ex "set \$WINDOW_A_MS = $WINDOW_A_MS"
  -ex "set \$WINDOW_B_MS = $WINDOW_B_MS"
  -ex "target extended-remote localhost:3333"
  -x "$GDBCMDS")

TIMEOUT_BIN=$(command -v timeout || command -v gtimeout || true)
if [ -n "$TIMEOUT_BIN" ]; then
  "$TIMEOUT_BIN" 300 "$GDB" "${GDB_ARGS[@]}" \
    || echo "hang_thread_walk: gdb exited non-zero / timed out"
else
  "$GDB" "${GDB_ARGS[@]}" \
    || echo "hang_thread_walk: gdb exited non-zero (see output above)"
fi

echo "hang_thread_walk: done (board serial capture in $SERIAL_LOG,"
echo "                        openocd log in hang-thread-walk-openocd.log)"
