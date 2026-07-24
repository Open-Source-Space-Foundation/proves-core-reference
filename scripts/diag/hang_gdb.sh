#!/usr/bin/env bash
# hang_gdb.sh -- v3 of the /keys boot-hang forensics (see INVESTIGATION.md).
#
# The register/QMI/stack capture (hang_forensics.tcl) ruled out a CPU fault, a
# lockup, and a wedged flash/XIP interface, and showed the core spinning with
# interrupts masked (PRIMASK=1), an off-boundary PC inside fs_open, an
# incoherent LR (idle), and a shallow garbage stack -- i.e. a software
# control-flow failure, not a clean fs mutex block. This step attaches GDB to
# the OpenOCD gdb server for a real DWARF backtrace + single-step, to decide
# between: (a) a wild/corrupted PC, (b) a Zephyr __ASSERT/SPIN_VALIDATE panic
# (a z_fatal_error/arch_system_halt frame would show), or (c) a genuine
# lock-spin. That distinction determines whether the littlefs->NVS rework is
# the right fix or a red herring.
#
# Read-only: halts and inspects, never writes flash or changes config. Safe to
# read code over SWD now that v2 confirmed XIP is healthy. Best-effort: if no
# suitable GDB is found on the runner it prints a notice and exits 0 (the CI
# step is `|| true` anyway) -- the register capture already stands on its own.
set -u

OCD=~/openocd/src/openocd
OCD_ARGS=(-s ~/openocd/tcl
  -f ~/openocd/tcl/interface/cmsis-dap.cfg
  -f ~/openocd/tcl/target/rp2350.cfg
  -c "adapter speed 5000")

# --- locate the symbol ELF (flight-software deployment, load addrs 0x1018_xxxx)
ELF=""
for cand in \
  build-artifacts/zephyr/fprime-zephyr-deployment \
  "$GITHUB_WORKSPACE"/build-artifacts/zephyr/fprime-zephyr-deployment; do
  [ -f "$cand" ] && { ELF="$cand"; break; }
done
if [ -z "$ELF" ]; then
  echo "hang_gdb: symbol ELF not found (build-artifacts/zephyr/fprime-zephyr-deployment); skipping."
  exit 0
fi
echo "hang_gdb: using ELF $ELF"

# --- locate an ARM / multiarch GDB
GDB=""
for cand in arm-zephyr-eabi-gdb gdb-multiarch arm-none-eabi-gdb; do
  command -v "$cand" >/dev/null 2>&1 && { GDB="$cand"; break; }
done
if [ -z "$GDB" ]; then
  GDB=$(ls ~/zephyr-sdk*/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb 2>/dev/null | head -1)
fi
if [ -z "$GDB" ]; then
  echo "hang_gdb: no arm/multiarch gdb on this runner (tried arm-zephyr-eabi-gdb,"
  echo "          gdb-multiarch, arm-none-eabi-gdb, ~/zephyr-sdk*). Skipping backtrace."
  exit 0
fi
echo "hang_gdb: using GDB $GDB"

# --- start OpenOCD: reset, free-run to the hang, halt, then KEEP the gdb server
#     alive (no `shutdown`) so GDB can attach to the halted core.
"$OCD" "${OCD_ARGS[@]}" \
  -c "init" \
  -c "reset run" \
  -c "sleep 8000" \
  -c "targets rp2350.cm0" \
  -c "halt" \
  -c "echo {hang_gdb: core halted, gdb server ready on :3333}" &
OCD_PID=$!
trap 'kill "$OCD_PID" 2>/dev/null' EXIT
# wait past the 8s free-run + halt before connecting
sleep 12

"$GDB" -q -nx -batch "$ELF" \
  -ex "set pagination off" \
  -ex "set confirm off" \
  -ex "target extended-remote localhost:3333" \
  -ex "echo \n==== core registers ====\n" \
  -ex "info registers" \
  -ex "echo \n==== masking / psr ====\n" \
  -ex "info registers primask basepri faultmask control xpsr" \
  -ex "echo \n==== DWARF backtrace (the decisive read) ====\n" \
  -ex "bt -full" \
  -ex "echo \n==== frame 0 detail ====\n" \
  -ex "info frame" \
  -ex "echo \n==== disassembly around PC ====\n" \
  -ex "x/16i \$pc-16" \
  -ex "echo \n==== single-step x8: does the PC advance, and to where? ====\n" \
  -ex "stepi" -ex "printf \"pc=%#x\\n\", \$pc" \
  -ex "stepi" -ex "printf \"pc=%#x\\n\", \$pc" \
  -ex "stepi" -ex "printf \"pc=%#x\\n\", \$pc" \
  -ex "stepi" -ex "printf \"pc=%#x\\n\", \$pc" \
  -ex "stepi" -ex "printf \"pc=%#x\\n\", \$pc" \
  -ex "stepi" -ex "printf \"pc=%#x\\n\", \$pc" \
  -ex "stepi" -ex "printf \"pc=%#x\\n\", \$pc" \
  -ex "stepi" -ex "printf \"pc=%#x\\n\", \$pc" \
  -ex "echo \n==== backtrace after stepping ====\n" \
  -ex "bt" \
  -ex "echo \n==== threads (openocd hwthread view) ====\n" \
  -ex "info threads" \
  -ex "detach" \
  || echo "hang_gdb: gdb session returned non-zero (see output above)"

echo "hang_gdb: done"
