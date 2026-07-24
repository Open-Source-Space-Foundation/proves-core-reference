# hang_forensics.tcl -- read-only SWD forensics for the /keys boot hang
# (branch hmac-to-storage, PR #472). See INVESTIGATION.md.
#
# Purpose: decide *why* cm0 is frozen at ~fs_open after the flash-size fix,
# BEFORE committing to the littlefs->NVS rework. Distinguishes:
#   (A) core faulted / locked up          -> CFSR/HFSR/DFSR/DHCSR non-zero
#   (B) QMI/XIP interface left wedged      -> QMI_DIRECT_CSR.EN=1 / bad M0_RFMT
#       by the flash erase/program dance      (flash-layer bug; NVS would ALSO
#                                              hang -> rework is wasted)
#   (C) cleanly blocked on an fs/lfs lock  -> no fault, XIP sane, stack shows a
#                                              k_mutex/k_sem wait (rework helps)
#
# Usage (CI or MOSAIC/GDS bench):
#   openocd -s ~/openocd/tcl \
#     -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
#     -c "adapter speed 5000" \
#     -f scripts/diag/hang_forensics.tcl
#
# SAFETY -- DO NOT read any 0x10xx_xxxx (XIP flash) address over SWD here. If
# the QMI/XIP interface is wedged (hypothesis B), an SWD read of a flash
# address can stall the debug adapter and lose the whole capture. Every read
# below targets the PPB (0xE000_xxxx), the QMI/XIP peripherals (0x400C/D_xxxx),
# or SRAM (0x2003_xxxx) only. The instruction at PC is already known from the
# ELF (fs_open begins with a 4-byte `stmdb` at 0x101864b6), so we never read
# code back over the wire.
#
# Memory-AP reads (mdw) work whether or not the core actually halts, so even a
# locked-up core still yields its fault registers and peripheral state. Each
# section is wrapped in `catch` so one failing read never aborts the rest.

proc rd {label addr} {
    if {[catch {set line [capture "mdw $addr"]} err]} {
        echo "  $label ($addr): <read failed: $err>"
    } else {
        echo "  $label ($addr): [string trim $line]"
    }
}

# Bare `reg`/`mdw` output does NOT reach the CI step stdout in batch (-c/-f)
# mode -- only `echo` and `capture` do (see run 30051402310, where the core-reg
# and stack blocks came back empty). Wrap every such command so its output is
# echoed into the captured log.
proc dump {cmd} {
    if {[catch {set out [capture $cmd]} err]} {
        echo "  \[$cmd]: <failed: $err>"
    } else {
        echo "  \[$cmd]: [string trim $out]"
    }
}

init

echo "=== reset + free-run so boot reaches the hang ==="
# Console-log diagnostic put the hang at ~1.16 s; 8 s leaves a wide margin and
# the PC sweep already proved the freeze is permanent (0 progress over 20 s),
# so a single halt is sufficient -- no need to sweep offsets again.
reset run
sleep 8000

targets rp2350.cm0
poll
# Tolerate a halt that never completes (lockup / bus stall): the memory reads
# below go through the debug MEM-AP and do not require the core to be halted.
catch {halt}
poll

echo ""
echo "=== CORE REGISTERS (cm0) -- expect pc ~= fs_open (0x101864b6) ==="
foreach r {pc lr sp msp psp xpsr r0 r1 r2 r3 r4 r5 r6 r7} {
    dump "reg $r"
}

echo ""
echo "=== INTERRUPT MASKING -- what is holding IRQs off? ==="
echo "    PRIMASK=1 => irq_lock via PRIMASK; BASEPRI!=0 => Zephyr BASEPRI mask."
echo "    Confirms the ISRPENDING-but-unserviced spin seen in run 30051402310."
foreach r {primask basepri faultmask control} {
    dump "reg $r"
}

echo ""
echo "=== FAULT / DEBUG STATUS (Cortex-M SCB, architectural addrs) ==="
echo "    non-zero CFSR/HFSR => a fault escalated; DHCSR bit19 (0x00080000)"
echo "    S_LOCKUP => core is locked up, PC is stale (rules out the fs-lock"
echo "    theory outright)."
rd "ICSR   " 0xE000ED04
rd "SHCSR  " 0xE000ED24
rd "CFSR   " 0xE000ED28
rd "HFSR   " 0xE000ED2C
rd "DFSR   " 0xE000ED30
rd "MMFAR  " 0xE000ED34
rd "BFAR   " 0xE000ED38
rd "DHCSR  " 0xE000EDF0

echo ""
echo "=== QMI / XIP PERIPHERAL STATE -- is XIP still restored after erase? ==="
echo "    QMI_DIRECT_CSR bit0 (EN, 0x1) SET => still in direct/serial mode, XIP"
echo "    NOT re-enabled -> next flash fetch stalls forever == the wedge. A"
echo "    clobbered M0_RFMT/M0_RCMD means the XIP read cmd config was lost."
rd "QMI_DIRECT_CSR" 0x400D0000
rd "QMI_M0_TIMING " 0x400D000C
rd "QMI_M0_RFMT   " 0x400D0010
rd "QMI_M0_RCMD   " 0x400D0014
rd "XIP_CTRL      " 0x400C8000
rd "XIP_STAT      " 0x400C8008

echo ""
echo "=== STACK DUMP for hand-unwind (SRAM only) ==="
echo "    Return addresses appear as 0x1018_xxxx / 0x1010_xxxx words on the"
echo "    stack; resolve them against zephyr.elf to reconstruct the call chain"
echo "    below fs_open (mount? create? a k_mutex_lock wait?). SP was a rock"
echo "    stable 0x20034410 across the whole PC sweep, so dump a fixed window"
echo "    from just below it (does not require the live reg read to succeed)."
dump "reg sp"
dump "mdw 0x20034380 96"

echo ""
echo "=== done ==="
catch { resume }
shutdown
