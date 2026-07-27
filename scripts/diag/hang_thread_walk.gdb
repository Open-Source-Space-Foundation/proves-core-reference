# hang_thread_walk.gdb -- v5 of the /keys boot-hang forensics (INVESTIGATION.md).
#
# The v4 fault-entry probe (hang_fault_bp.*) established there is NO CPU fault:
# breakpoints on z_arm_usage_fault / z_arm_fault never hit, and a clean halt of
# the board shows cm0 in arch_cpu_idle with CFSR=HFSR=0.  The open question was
# whether the scheduler had stopped making progress with every thread blocked
# forever.
#
# This probe answers the two questions that state raises:
#   1. Is the system clock still running?  (compare cycle_count / curr_tick and
#      the live SysTick registers across two halts several seconds apart)
#   2. Who is blocked, and on what?  (walk _kernel.threads and, for each
#      swapped-out thread, recover its resume PC/LR from the saved PSP frame)
#
# Driven by hang_thread_walk.sh, which has connected to the OpenOCD gdb server.
# All run/halt sequencing happens below via `monitor`, so gdb never believes the
# target is running -- hence the explicit register-cache flush after each halt
# and the stack/code caches disabled by the driver script.
#
# Build facts relied on (build-fprime-automatic-zephyr/zephyr/.config):
#   CONFIG_THREAD_MONITOR=y        -> _kernel.threads list + k_thread.entry exist
#   CONFIG_THREAD_STACK_INFO=y     -> stack_info.start/size are valid
#   CONFIG_THREAD_NAME is not set  -> no k_thread.name; we symbolize entry.pEntry
#   CONFIG_USE_SWITCH is not set   -> classic Cortex-M PendSV swap.  Note the
#                                     callee-saved regs live in the k_thread
#                                     (struct _callee_saved = v1-v8 + psp), NOT
#                                     on the stack, so callee_saved.psp points
#                                     straight at the hardware exception frame:
#                                     [r0,r1,r2,r3,r12,lr,pc,xpsr]
#   CONFIG_FPU is not set          -> that frame is exactly 0x20 bytes
#   CONFIG_MP_MAX_NUM_CPUS=1       -> current thread is _kernel.cpus[0].current
#   CONFIG_TICKLESS_KERNEL=y       -> SysTick is reloaded per-timeout (last_load)
#   CONFIG_CORTEX_M_SYSTICK_64BIT_CYCLE_COUNTER=y -> cycle_count is 64-bit

# Cortex-M33 register block addresses used below (read as words):
#   0xE000E010 SYST_CSR   0xE000E014 SYST_RVR   0xE000E018 SYST_CVR
#   0xE000E100 NVIC_ISER0 0xE000E200 NVIC_ISPR0
#   0xE000ED04 ICSR       0xE000ED24 SHCSR      0xE000ED28 CFSR  0xE000ED2C HFSR

define hw_snapshot
  printf "-- kernel time base --\n"
  printf "   cycle_count      = %llu\n", (unsigned long long)cycle_count
  printf "   announced_cycles = %llu\n", (unsigned long long)announced_cycles
  printf "   curr_tick        = %lld\n", (long long)curr_tick
  printf "   last_load        = %#lx\n", (unsigned long)last_load
  printf "-- SysTick (live peripheral) --\n"
  printf "   SYST_CSR = %#010lx   (bit0 ENABLE, bit1 TICKINT, bit16 COUNTFLAG)\n", \
         *(unsigned long*)0xE000E010
  printf "   SYST_RVR = %#010lx   SYST_CVR = %#010lx\n", \
         *(unsigned long*)0xE000E014, *(unsigned long*)0xE000E018
  printf "-- interrupt state --\n"
  printf "   PRIMASK=%#lx  BASEPRI=%#lx  FAULTMASK=%#lx  CONTROL=%#lx\n", \
         (unsigned long)$primask, (unsigned long)$basepri, \
         (unsigned long)$faultmask, (unsigned long)$control
  printf "   ICSR     = %#010lx   (bit22 ISRPENDING, bits[8:0] VECTACTIVE)\n", \
         *(unsigned long*)0xE000ED04
  printf "   NVIC_ISER0=%#010lx  NVIC_ISPR0=%#010lx  SHCSR=%#010lx\n", \
         *(unsigned long*)0xE000E100, *(unsigned long*)0xE000E200, \
         *(unsigned long*)0xE000ED24
  printf "   CFSR     = %#010lx   HFSR     = %#010lx  (both 0 => no CPU fault)\n", \
         *(unsigned long*)0xE000ED28, *(unsigned long*)0xE000ED2C
  printf "-- live core --\n"
  printf "   pc=%#lx  sp=%#lx  msp=%#lx  psp=%#lx\n", \
         (unsigned long)$pc, (unsigned long)$sp, \
         (unsigned long)$msp, (unsigned long)$psp
  printf "   pc -> "
  info symbol $pc
  printf "   backtrace of the live context:\n"
  bt 12
  printf "   -- call chain (text-looking words above live sp) --\n"
  stack_ras $sp 0x200
end

# kernel/timeout.c's static list -- the answer to "is anything still
# scheduled to wake, and when".  Kept in its own command (invoked after
# thread_walk) so a problem here never costs us the thread data.
define timeout_walk
  # An empty sys_dlist_t points at itself, so head == &timeout_list means
  # nothing at all is waiting on the clock.
  printf "-- timeout queue (kernel/timeout.c timeout_list @ %#lx) --\n", \
         (unsigned long)&timeout_list
  printf "   head = %#lx%s\n", (unsigned long)timeout_list.head, \
         (timeout_list.head == &timeout_list ? \
          "   (EMPTY: nothing is waiting on time)" : "   (timeouts pending)")
  # dticks is a *delta* chain: entry N fires `sum(dticks[0..N])` ticks after the
  # last announcement.  A head whose remaining delta never shrinks between the
  # two halts is the smoking gun for "the clock counts but nothing is announced
  # to the timeout layer".
  set $to_n = 0
  set $to_sum = (long long)0
  set $to_p = (struct _timeout *)timeout_list.head
  while $to_p != (struct _timeout *)&timeout_list && $to_n < 12
    set $to_sum = $to_sum + (long long)$to_p->dticks
    printf "   [%d] _timeout @ %#lx  dticks=%lld  (fires in %lld ticks = %lld ms)\n", \
           $to_n, (unsigned long)$to_p, (long long)$to_p->dticks, $to_sum, \
           $to_sum * 1000 / 10000
    printf "        fn = %#lx -> ", (unsigned long)$to_p->fn
    info symbol $to_p->fn
    set $to_p = (struct _timeout *)$to_p->node.next
    set $to_n = $to_n + 1
  end
  printf "   (%d timeouts queued)\n", $to_n
end

# The downlink chain, per Com subtopology instance.  Every stage of it waits on
# a status handed back from the stage below:
#     comQueue -> spacePacketFramer -> aggregator -> framer -> comStub -> driver
#     comStub.comStatusOut -> framer -> aggregator -> spacePacketFramer -> comQueue
# so a status that never comes back parks the whole chain and the link goes
# silent while the rest of the system stays perfectly healthy.  This dump says
# which stage is parked and whether frames are moving at all.
#   ComQueue.m_state          READY | WAITING (WAITING = sent, awaiting status)
#   ComAggregator.m_allow_timeout   false => in WAIT_STATUS, discarding timeouts
#   TmFramer.m_masterFrameCount     increments per frame emitted downstream --
#                                   compare across the two halts: not advancing
#                                   means nothing is being framed at all
#   ComStub.m_reinitialize          true => still waiting for a drvConnected
# NOTE: ComQueue::run only publishes queue-depth telemetry and is NOT what
# drives the dequeue, so `run` being unconnected cannot cause silence.
# NOTE: only the UART subtopology instantiates a comStub; the LoRa one reaches
# its radio by another path, so `ComCcsdsLora::comStub` does not exist.
define com_state
  printf "-- downlink chain state --\n"
  printf "   %-14s %-9s %-14s %-14s %s\n", \
         "instance", "ComQueue", "Aggregator", "TmFramer", "ComStub"
  com_state_one ComCcsdsUart
  printf "   %-14s %-9s %-14s %-14s %s\n", "", "", "", "", ""
  com_state_one ComCcsdsLora
  printf "   ComCcsdsUart::comStub: reinitialize=%d retry_count=%d\n", \
         (int)ComCcsdsUart::comStub.m_reinitialize, \
         (int)ComCcsdsUart::comStub.m_retry_count
end

define com_state_one
  printf "   %-14s ", "$arg0"
  # ComQueue::SendState: READY=0, WAITING=1 (ComQueue.hpp:103).  Compared
  # numerically because gdb loses the Svc:: enum context across the reconnect.
  printf "%-9s ", ($arg0::comQueue.m_state == 0 ? "READY" : "*WAITING*")
  printf "%-14s ", ($arg0::aggregator.m_allow_timeout ? \
                    "FILL" : "*WAIT_STATUS*")
  printf "mfc=%-3d vfc=%-3d  ", (int)$arg0::framer.m_masterFrameCount, \
         (int)$arg0::framer.m_virtualFrameCount
  set $cs_mfc = (unsigned long)$arg0::framer.m_masterFrameCount
end

# Decode _thread_base.thread_state (include/zephyr/kernel_structs.h:52-72).
define state_bits
  set $st = (unsigned long)$arg0
  printf "%#04lx [", $st
  if $st == 0
    printf "READY/RUNNING"
  end
  if $st & 0x01
    printf "DUMMY "
  end
  if $st & 0x02
    printf "PENDING "
  end
  if $st & 0x04
    printf "SLEEPING "
  end
  if $st & 0x08
    printf "DEAD "
  end
  if $st & 0x10
    printf "SUSPENDED "
  end
  if $st & 0x20
    printf "ABORTING "
  end
  if $st & 0x40
    printf "SUSPENDING "
  end
  if $st & 0x80
    printf "QUEUED "
  end
  printf "]"
end

# Symbolize every word in [$arg0, $arg0+$arg1) that looks like a Thumb return
# address into .text -- a hand-rolled unwind.  Used instead of rewinding gdb's
# $pc/$sp into each blocked thread: that writes core registers and, when a frame
# is unrecoverable, wedges gdb ("attempt to assign to an unmodifiable value").
# This is purely read-only and works no matter how mangled the frame is.
define stack_ras
  set $ra_p = (unsigned long)$arg0
  set $ra_e = (unsigned long)$arg0 + (unsigned long)$arg1
  set $ra_n = 0
  while $ra_p < $ra_e && $ra_n < 20
    set $ra_w = *(unsigned long*)$ra_p
    # Thumb code pointer inside this image's .text region.
    if ($ra_w & 1) && $ra_w > (unsigned long)&__text_region_start && $ra_w < (unsigned long)&__text_region_end
      printf "       +%#04lx  %#010lx  ", $ra_p - (unsigned long)$arg0, $ra_w
      info symbol $ra_w - 1
      set $ra_n = $ra_n + 1
    end
    set $ra_p = $ra_p + 4
  end
  if $ra_n == 0
    printf "       (no text-looking return addresses in this window)\n"
  end
end

# Walk the CONFIG_THREAD_MONITOR list of every thread in the system.  For each
# swapped-out thread recover where it will resume: callee_saved.psp points at
# the hardware exception frame PendSV entry pushed, so the stacked LR is at
# +0x14 and the stacked PC at +0x18 (callee regs v1-v8 are in the k_thread).
#
# $walk_sum accumulates each thread's CONFIG_SCHED_THREAD_USAGE cycle counter
# (base.usage.total), which is monotonic: comparing it between the two halts is
# a one-number answer to "did ANY thread get CPU time?".  Do NOT use saved
# psp/resume-PC for this -- a healthy thread that blocks at the same line every
# cycle reproduces byte-identical values and would read as frozen.
define thread_walk
  set $cur = _kernel.cpus[0].current
  set $t = _kernel.threads
  set $n = 0
  set $walk_sum = (unsigned long long)0
  printf "-- thread walk (current = %#lx) --\n", (unsigned long)$cur
  while $t != 0 && $n < 32
    printf "\n [%d] k_thread @ %#lx%s\n", $n, (unsigned long)$t, \
           ($t == $cur ? "   <== CURRENT" : "")
    printf "     entry     = %#lx  -> ", (unsigned long)$t->entry.pEntry
    info symbol $t->entry.pEntry
    # CONFIG_THREAD_NAME is off, so all 21 F' task threads share one entry
    # symbol (Os::Zephyr::Task::zephyrEntryWrapper).  The entry *argument* is
    # the per-task pointer, which lands inside the owning F' component object
    # -- symbolizing it is what actually names the thread.
    printf "     arg       = %#lx  -> ", (unsigned long)$t->entry.parameter1
    info symbol $t->entry.parameter1
    printf "     state     = "
    state_bits $t->base.thread_state
    printf "   prio=%d  preempt=%#x\n", (int)$t->base.prio, \
           (unsigned int)$t->base.preempt
    printf "     pended_on = %#lx%s\n", (unsigned long)$t->base.pended_on, \
           ($t->base.pended_on != 0 ? "   (blocked on a wait queue)" : "")
    printf "     timeout   = dticks=%lld node.next=%#lx%s\n", \
           (long long)$t->base.timeout.dticks, \
           (unsigned long)$t->base.timeout.node.next, \
           ($t->base.timeout.node.next != 0 ? "   (queued in _kernel.timeouts)" : "   (NO timeout armed)")

    printf "     cpu cycles= %llu (base.usage.total)\n", \
           (unsigned long long)$t->base.usage.total
    set $walk_sum = $walk_sum + (unsigned long long)$t->base.usage.total
    set $sbase = (unsigned long)$t->stack_info.start
    set $ssize = (unsigned long)$t->stack_info.size
    set $tpsp  = (unsigned long)$t->callee_saved.psp
    printf "     stack     = base=%#lx size=%#lx top=%#lx\n", \
           $sbase, $ssize, $sbase + $ssize
    printf "     saved psp = %#lx", $tpsp
    if $tpsp < $sbase
      printf "   *** BELOW STACK BASE == OVERFLOW ***\n"
    else
      if $tpsp > $sbase + $ssize
        printf "   *** ABOVE STACK TOP (wrong stack / not yet swapped) ***\n"
      else
        printf "   used=%#lx of %#lx (%lu%% headroom left)\n", \
               ($sbase + $ssize - $tpsp), $ssize, \
               (unsigned long)(($tpsp - $sbase) * 100 / $ssize)
      end
    end

    # Resume PC/LR are only meaningful for a thread that is actually swapped
    # out with a PendSV frame on its own stack.  Skip the running thread (its
    # live $pc/$sp were printed by hw_snapshot) and any bogus psp.
    if $t != $cur && $tpsp >= $sbase && $tpsp + 0x20 <= $sbase + $ssize
      set $rlr = *(unsigned long*)($tpsp + 0x14)
      set $rpc = *(unsigned long*)($tpsp + 0x18)
      printf "     resume LR = %#lx  -> ", $rlr
      info symbol $rlr
      printf "     resume PC = %#lx  -> ", $rpc
      info symbol $rpc
      echo      -- call chain (text-looking words above the exception frame) --\n
      # gdb splits user-command arguments on whitespace, so an expression like
      # ($tpsp + 0x20) would arrive as three separate args -- precompute.
      set $rs_a = $tpsp + 0x20
      set $rs_l = ($sbase + $ssize) - $rs_a
      stack_ras $rs_a $rs_l
    end

    set $t = $t->next_thread
    set $n = $n + 1
  end
  printf "\n-- %d threads walked; total CPU cycles across all threads = %llu --\n", \
         $n, (unsigned long long)$walk_sum
end

# OpenOCD forwards its own log to the attached gdb, which interleaves
# "[rp2350.cm1] halted due to debug-request" mid-printf and shreds the report.
# Send it to a file instead (hang_thread_walk.sh prints the path).
monitor log_output hang-thread-walk-openocd.log

echo \n================ RESET + FREE-RUN INTO THE HANG ================\n
monitor reset halt
monitor resume
echo hang_thread_walk: running to reach the failed state (window A)...\n
eval "monitor sleep %d", $WINDOW_A_MS
monitor halt
# The run/halt above went through `monitor`, so gdb still believes the target
# never moved and would serve register values cached from the reset halt (this
# bit the first version of this script: it reported pc=z_arm_reset at a halt
# 12s into the run).  Reconnecting is the reliable resync: gdb re-queries the
# stop reason and gets the true state.  (`monitor gdb sync` + `stepi` is the
# usual recipe but is not safe here -- when the halt lands mid-ISR that stepi
# can resume the target, after which every later read fails with "Cannot
# execute this command while the target is running".)  Convenience variables
# survive the reconnect, so the A/B comparison below is unaffected.
detach
target extended-remote localhost:3333
maintenance flush register-cache

echo \n================ HALT A ================\n
hw_snapshot
set $A_cycles   = (unsigned long long)cycle_count
set $A_tick     = (long long)curr_tick
set $A_cvr      = *(unsigned long*)0xE000E018
set $A_current  = (unsigned long)_kernel.cpus[0].current
thread_walk
timeout_walk
com_state
set $A_sum = $walk_sum

eval "echo \\n================ FREE-RUN %d ms ================\\n", $WINDOW_B_MS
monitor resume
eval "monitor sleep %d", $WINDOW_B_MS
monitor halt
# The run/halt above went through `monitor`, so gdb still believes the target
# never moved and would serve register values cached from the reset halt (this
# bit the first version of this script: it reported pc=z_arm_reset at a halt
# 12s into the run).  Reconnecting is the reliable resync: gdb re-queries the
# stop reason and gets the true state.  (`monitor gdb sync` + `stepi` is the
# usual recipe but is not safe here -- when the halt lands mid-ISR that stepi
# can resume the target, after which every later read fails with "Cannot
# execute this command while the target is running".)  Convenience variables
# survive the reconnect, so the A/B comparison below is unaffected.
detach
target extended-remote localhost:3333
maintenance flush register-cache

echo \n================ HALT B ================\n
hw_snapshot
set $B_cycles  = (unsigned long long)cycle_count
set $B_tick    = (long long)curr_tick
set $B_cvr     = *(unsigned long*)0xE000E018
set $B_current = (unsigned long)_kernel.cpus[0].current
thread_walk
timeout_walk
com_state
set $B_sum = $walk_sum

echo \n================ VERDICT ================\n
printf "cycle_count  A=%llu  B=%llu  delta=%lld\n", \
       $A_cycles, $B_cycles, (long long)($B_cycles - $A_cycles)
printf "curr_tick    A=%lld  B=%lld  delta=%lld\n", \
       $A_tick, $B_tick, ($B_tick - $A_tick)
printf "SYST_CVR     A=%#lx  B=%#lx  (free-running counter; equal is suspicious\n", \
       $A_cvr, $B_cvr
printf "              but not conclusive -- it wraps every RVR ticks)\n"
if $B_cycles == $A_cycles && $B_tick == $A_tick
  echo *** CLOCK IS FROZEN: no ticks announced across 4s -- SysTick ISR is not\n
  echo *** running.  Look at PRIMASK/BASEPRI/ICSR above and at whoever masked.\n
else
  echo *** CLOCK IS ALIVE: ticks still advancing, so the hang is a scheduler /\n
  echo *** blocked-thread problem, not a dead timer.  The thread that should be\n
  echo *** running is blocked -- see its pended_on + backtrace above.\n
end
printf "current thread  A=%#lx  B=%#lx  ->  %s\n", $A_current, $B_current, \
       ($A_current == $B_current ? "SAME (no context switch in 4s)" : "changed")
printf "thread CPU cyc  A=%llu  B=%llu  delta=%llu  ->  %s\n", \
       (unsigned long long)$A_sum, (unsigned long long)$B_sum, \
       (unsigned long long)($B_sum - $A_sum), \
       ($A_sum == $B_sum ? \
        "*** NO thread got any CPU time in 4s: the system really is wedged" : \
        "threads are still being scheduled: the system is RUNNING")

echo \n================ done (detaching; board left halted) ================\n
detach
