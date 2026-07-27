#!/usr/bin/env bash
# hang_fault_bp.sh -- v4 of the /keys boot-hang forensics (see INVESTIGATION.md).
#
# All prior forensics (hang_forensics.tcl / hang_gdb.sh) halted the board AFTER
# it was already spinning in the masked `b .` fatal-halt loop -- i.e. after
# z_arm_fatal_error ran -- so the backtrace was incoherent (lr=0x1019, garbage
# stack).  This step instead sets a HARDWARE breakpoint on the UsageFault vector
# entry (z_arm_usage_fault), resets, and lets it hit at ~8s, catching the fault
# with the exception frame fresh and EXC_RETURN still in LR.  From that frame it
# reads the true faulting PC, the caller LR, and the pre-fault SP, and compares
# SP to the faulting thread's stack bounds -- deciding STACK OVERFLOW vs. a
# WILD/CORRUPTED POINTER directly.  See hang_fault_bp.gdb for the read logic.
#
# Local bench only (not CI).  Drives the board over a Raspberry Pi Debug Probe
# (CMSIS-DAP SWD).  The two USB CDC ttys involved:
#   BOARD_TTY  /dev/tty.usbmodem3101  - the target's own USB CDC (F'/GDS link);
#                                       telemetry goes silent here at the hang
#   PROBE_TTY  /dev/tty.usbmodem102   - the Debug Probe's UART bridge
# OpenOCD reaches SWD via the probe's CMSIS-DAP USB interface (not a tty); the
# ttys are used only to correlate/timestamp the hang.
#
# Read-mostly: it resets+halts and reads state; the only writes are the GDB
# breakpoint and a register rewind for the reconstructed backtrace, both on a
# target that is reset at the start of the run.  Never writes flash or config.
set -u

BOARD_TTY=${BOARD_TTY:-/dev/tty.usbmodem3101}
PROBE_TTY=${PROBE_TTY:-/dev/tty.usbmodem102}

# Which fault entry to break on.  Default z_arm_usage_fault (the UsageFault
# vector -- LR still holds EXC_RETURN there).  Set FAULT_SYM=z_arm_fault to
# instead catch the common C handler, for faults that don't route through the
# UsageFault vector (e.g. a HardFault escalation); its args carry EXC_RETURN.
FAULT_SYM=${FAULT_SYM:-z_arm_usage_fault}
if [ "$FAULT_SYM" = "z_arm_fault" ]; then
  FAULT_ARGS=1   # read exc_return/msp/psp from r2/r0/r1 (see hang_fault_bp.gdb)
else
  FAULT_ARGS=0   # read exc_return from LR, msp/psp live
  [ "$FAULT_SYM" != "z_arm_usage_fault" ] && \
    echo "hang_fault_bp: warning: FAULT_SYM=$FAULT_SYM is unrecognized; assuming LR holds EXC_RETURN (vector-entry mode)"
fi
# Break at the exact address (`*`) so GDB does not skip a prologue and clobber
# the argument registers before we read them.
BP_SPEC="*$FAULT_SYM"

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
[ -x "$OOCD_HOME/src/openocd" ] || { echo "hang_fault_bp: openocd not found (set OOCD_HOME to the raspberrypi openocd checkout)"; exit 1; }
OCD="$OOCD_HOME/src/openocd"
OCD_ARGS=(-s "$OOCD_HOME/tcl"
  -f "$OOCD_HOME/tcl/interface/cmsis-dap.cfg"
  -f "$OOCD_HOME/tcl/target/rp2350.cfg"
  -c "adapter speed 5000")

HERE=$(cd "$(dirname "$0")" && pwd)
GDBCMDS="$HERE/hang_fault_bp.gdb"
[ -f "$GDBCMDS" ] || { echo "hang_fault_bp: missing $GDBCMDS"; exit 1; }

# --- symbol ELF (local flight build; load addrs 0x1018_xxxx) ---
ELF=""
for cand in \
  build-fprime-automatic-zephyr/zephyr/zephyr.elf \
  build-artifacts/zephyr/fprime-zephyr-deployment; do
  [ -f "$cand" ] && { ELF="$cand"; break; }
done
[ -z "$ELF" ] && { echo "hang_fault_bp: symbol ELF not found; run 'make generate build' first."; exit 1; }
echo "hang_fault_bp: ELF   $ELF"
echo "hang_fault_bp: board $BOARD_TTY   probe $PROBE_TTY"
echo "hang_fault_bp: break $BP_SPEC (FAULT_ARGS=$FAULT_ARGS)"

# --- ARM gdb ---
GDB=""
for cand in arm-zephyr-eabi-gdb gdb-multiarch arm-none-eabi-gdb; do
  command -v "$cand" >/dev/null 2>&1 && { GDB="$cand"; break; }
done
[ -z "$GDB" ] && GDB=$(ls ~/zephyr-sdk*/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb 2>/dev/null | head -1)
[ -z "$GDB" ] && { echo "hang_fault_bp: no arm/multiarch gdb found."; exit 1; }
echo "hang_fault_bp: gdb   $GDB"

# --- optional: capture the board CDC so telemetry-going-silent is timestamped ---
SERIAL_LOG="board-serial.log"
SNIFF_PID=""
if [ -e "$BOARD_TTY" ]; then
  ( cat "$BOARD_TTY" > "$SERIAL_LOG" 2>/dev/null ) &
  SNIFF_PID=$!
  echo "hang_fault_bp: sniffing $BOARD_TTY -> $SERIAL_LOG"
else
  echo "hang_fault_bp: note: $BOARD_TTY not present; skipping serial sniff"
fi

# --- OpenOCD: init + keep the gdb server up; GDB drives reset/breakpoint ---
"$OCD" "${OCD_ARGS[@]}" -c "init" -c "echo {hang_fault_bp: gdb server on :3333}" &
OCD_PID=$!
cleanup() { [ -n "$SNIFF_PID" ] && kill "$SNIFF_PID" 2>/dev/null; kill "$OCD_PID" 2>/dev/null; }
trap cleanup EXIT
sleep 2

# Connect, reset+halt, and arm the HW breakpoint here (before boot runs), then
# hand off to the command file which continues to the fault and reads the frame.
# Guard with a timeout so a fault that never fires (continue blocks forever)
# doesn't wedge the run.
GDB_ARGS=(-q -nx -batch "$ELF"
  -ex "set pagination off"
  -ex "set confirm off"
  -ex "set print pretty on"
  -ex "target extended-remote localhost:3333"
  -ex "monitor reset halt"
  -ex "hbreak $BP_SPEC"
  -ex "set \$FAULT_ARGS = $FAULT_ARGS"
  -x "$GDBCMDS")

TIMEOUT_BIN=$(command -v timeout || command -v gtimeout || true)
if [ -n "$TIMEOUT_BIN" ]; then
  "$TIMEOUT_BIN" 90 "$GDB" "${GDB_ARGS[@]}" \
    || echo "hang_fault_bp: gdb exited non-zero / timed out (fault may not have fired within 90s)"
else
  echo "hang_fault_bp: note: no timeout(1); gdb 'continue' will block until the fault fires."
  "$GDB" "${GDB_ARGS[@]}" \
    || echo "hang_fault_bp: gdb exited non-zero (see output above)"
fi

echo "hang_fault_bp: done (board serial capture in $SERIAL_LOG)"
