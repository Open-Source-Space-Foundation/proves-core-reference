# hang_fault_bp.gdb -- read the pre-fault (hardware-stacked) CPU state once a
# fault breakpoint has hit, before z_arm_fatal_error masks IRQs and spins in
# `b .`.  Driven by hang_fault_bp.sh, which has already: connected to the gdb
# server, `monitor reset halt`, armed the HW breakpoint, and set $FAULT_ARGS.
# See INVESTIGATION.md "Next diagnostic (v4): catch the fault at entry".
#
# Two entry points, selected by hang_fault_bp.sh via FAULT_SYM -> $FAULT_ARGS:
#   $FAULT_ARGS==0  break at z_arm_usage_fault (the UsageFault *vector* entry):
#                   the HW-stacked frame is fresh and LR still holds EXC_RETURN,
#                   and live $msp/$psp are the pre-fault stack pointers.
#   $FAULT_ARGS==1  break at *z_arm_fault (the common C handler) instead, for
#                   faults that don't route through z_arm_usage_fault.  Its args
#                   carry the state: z_arm_fault(msp=r0, psp=r1, exc_return=r2,
#                   callee=r3) -- confirmed in this build's fault.c:1025 -- so we
#                   take EXC_RETURN/msp/psp from r2/r0/r1 (LR is stale here).
#
# Build facts relied on (build-fprime-automatic-zephyr/zephyr/.config):
#   CONFIG_FPU is not set          -> plain 8-word (0x20) exception frame
#   CONFIG_MP_MAX_NUM_CPUS=1       -> current thread is _kernel.cpus[0].current
#   CONFIG_THREAD_STACK_INFO=y     -> stack_info.start/size are valid
#   CONFIG_THREAD_NAME is not set  -> k_thread has no .name member (don't read it)

echo \n==== running to the fault breakpoint (expected ~8s into boot) ====\n
continue

echo \n==== FAULT CAUGHT -- pre-halt, exception frame intact ====\n
info registers lr primask basepri control xpsr

# Recover EXC_RETURN and the pre-fault MSP/PSP for whichever entry we stopped at.
if $FAULT_ARGS
  set $exc   = (unsigned long)$r2
  set $msp_v = (unsigned long)$r0
  set $psp_v = (unsigned long)$r1
  printf "entry=z_arm_fault (args): exc_return=r2  msp=r0  psp=r1\n"
else
  set $exc   = (unsigned long)$lr
  set $msp_v = (unsigned long)$msp
  set $psp_v = (unsigned long)$psp
  printf "entry=z_arm_usage_fault (vector): exc_return=lr  msp/psp live\n"
end

# EXC_RETURN bit2 selects the stack the CPU pushed the exception frame onto:
#   0 -> MSP (fault happened in handler mode)   1 -> PSP (fault in a thread)
set $usepsp = ($exc >> 2) & 1
set $frame  = $usepsp ? $psp_v : $msp_v
printf "exc_return=%#lx  pre-fault stack=%s  frame_sp=%#lx\n", \
       $exc, ($usepsp ? "PSP(thread)" : "MSP(handler)"), $frame

echo \n==== stacked exception frame = the exact pre-fault CPU state ====\n
set $sr0   = *(unsigned long*)($frame+0x00)
set $sr1   = *(unsigned long*)($frame+0x04)
set $sr2   = *(unsigned long*)($frame+0x08)
set $sr3   = *(unsigned long*)($frame+0x0c)
set $sr12  = *(unsigned long*)($frame+0x10)
set $slr   = *(unsigned long*)($frame+0x14)
set $spc   = *(unsigned long*)($frame+0x18)
set $sxpsr = *(unsigned long*)($frame+0x1c)
printf "  r0=%#lx r1=%#lx r2=%#lx r3=%#lx r12=%#lx\n", $sr0,$sr1,$sr2,$sr3,$sr12
printf "  stacked LR (caller/return)  = %#lx\n", $slr
printf "  stacked PC (faulting instr) = %#lx\n", $spc
printf "  stacked xPSR                = %#lx\n", $sxpsr
echo -- symbolize the pre-fault PC and LR --\n
printf "  faulting PC -> "
info symbol $spc
printf "  stacked  LR -> "
info symbol $slr

echo \n==== why: CFSR / UFSR (UsageFault status @ 0xE000ED28) ====\n
# UFSR = upper halfword of CFSR.  Key bits for a wild jump:
#   bit16 UNDEFINSTR : jumped into non-code / bad opcode
#   bit17 INVSTATE   : Thumb (EPSR.T) bit clear -> branched to an even/data addr
#   bit18 INVPC      : bad EXC_RETURN / integrity check
# INVSTATE or UNDEFINSTR here == executed a data/garbage address (matches the
# observed pc=0x20010480 inside the fileManager object).
x/1xw 0xE000ED28

echo \n==== stack-overflow test: is frame_sp below the faulting thread's stack? ====\n
set $thr   = _kernel.cpus[0].current
set $sbase = (unsigned long)$thr->stack_info.start
set $ssize = (unsigned long)$thr->stack_info.size
printf "  current k_thread @ %#lx\n", (unsigned long)$thr
printf "  stack: base=%#lx  size=%#lx  top=%#lx\n", $sbase, $ssize, $sbase+$ssize
printf "  frame_sp=%#lx  ->  %s\n", $frame, \
       ($frame < $sbase ? "*** SP BELOW STACK BASE == STACK OVERFLOW ***" : \
        ($frame > $sbase+$ssize ? "*** SP ABOVE STACK TOP (wrong stack?) ***" : \
         "within stack extent (points to wild pointer, not overflow)"))

echo \n==== reconstructed backtrace of the FAULTING thread ====\n
# Rewind GDB's view to the pre-fault frame so `bt` unwinds the culprit rather
# than the fault handler.  Frame is 0x20 bytes; xPSR bit9 set => +4 align pad.
# NOTE: this writes core registers on the (about-to-be-reset) target.
set $pad = (($sxpsr >> 9) & 1) ? 4 : 0
set $sp  = $frame + 0x20 + $pad
set $pc  = $spc
set $lr  = $slr
bt

echo \n==== raw stack window above frame_sp (find real 0x10xx return addrs) ====\n
x/64xw $frame

echo \n==== done (detaching; board left halted) ====\n
detach
