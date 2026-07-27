# downlink_trace.gdb -- v6 of the /keys CI-failure forensics (INVESTIGATION.md).
#
# v5 (hang_thread_walk.*) proved the firmware is healthy and localised the
# failure to the downlink: at t=12s both Com subtopologies show
#   ComQueue.m_state           = WAITING   (on BOTH the UART and LoRa paths)
#   TmFramer.m_masterFrameCount = 0        (on BOTH -- no TM frame has EVER
#                                           been framed, on either link)
#   ComCcsdsUart::comStub.m_reinitialize = 0 (the driver's ready DID arrive and
#                                             comStub DID emit its one status)
#
# ComQueue is constructed in WAITING (ComQueue.cpp:35) and only ever reaches
# READY via comStatusIn carrying SUCCESS (ComQueue.cpp:236-247).  Until then it
# never dequeues, so nothing is framed and the link is silent from boot.  The
# status has to travel
#     comStub.comStatusOut -> framer -> aggregator -> spacePacketFramer
#                          -> comQueue.comStatusIn
# and it demonstrably reaches the aggregator (its m_allow_timeout is true, i.e.
# the FILL state) but not comQueue.  This probe watches that whole path live
# from reset and reports exactly where the status dies.
#
# Read-only apart from breakpoints/watchpoints, all set on a target that is
# reset at the start of the run.  Driven by downlink_trace.sh.

monitor log_output downlink-trace-openocd.log

echo \n================ arming the downlink status path ================\n
monitor reset halt

# Every stage that must forward the status upward.  Plain linespec form, NOT
# `*func`: the `*` forces expression parsing, which needs Svc::ComStub as a
# *type* in the current context and fails with "No type ComStub within class or
# namespace Svc" before the program has run.  Letting gdb skip the prologue is
# fine here because the arguments are read with `info args` (DWARF locations)
# rather than out of raw registers.
#
# ComAggregator::preamble is the one that matters most: it is the F' active
# component preamble, run on the aggregator's own thread when tasks start, and
# it is the ONLY place the aggregator emits an unprovoked comStatusOut
# (ComAggregator.cpp:24-27).  Its other comStatusOut is in doFill, which needs
# data -- and data cannot flow until ComQueue is released.  So if preamble
# never runs, or its status never reaches ComQueue, the chain deadlocks from
# boot exactly as observed.
hbreak Svc::ComStub::drvConnected_handler
hbreak Svc::ComAggregator::preamble
hbreak Svc::ComAggregator::comStatusIn_handler
hbreak Svc::ComQueue::comStatusIn_handler
info breakpoints

# Report a stop, then keep going.  $stops bounds the run so a fast-repeating
# hit cannot spin forever.
set $stops = 0
set $limit = 14

echo \n================ running to topology setup ================\n
continue
printf "\n---- reached %s; arming the ComQueue.m_state watchpoint ----\n", "drvConnected"
# The gate itself.  A hardware watchpoint fires on every write, so "never
# fires again" is itself an answer, and each hit names the writer.  Armed here
# rather than at reset because bss-zeroing and ComQueue's own constructor write
# it during early boot and drown the trace in noise.
watch ComCcsdsUart::comQueue.m_state

echo \n================ tracing the status path ================\n
while $stops < $limit
  continue
  set $stops = $stops + 1
  printf "\n---- stop %d ----------------------------------------------\n", $stops
  printf "  pc = %#lx  -> ", (unsigned long)$pc
  info symbol $pc
  printf "  ComCcsdsUart::comQueue.m_state = %d (0=READY 1=WAITING)\n", \
         (int)ComCcsdsUart::comQueue.m_state
  printf "  ComCcsdsUart::aggregator FILL? %d   comStub.reinit=%d\n", \
         (int)ComCcsdsUart::aggregator.m_allow_timeout, \
         (int)ComCcsdsUart::comStub.m_reinitialize
  printf "  TmFramer mfc=%d vfc=%d\n", \
         (int)ComCcsdsUart::framer.m_masterFrameCount, \
         (int)ComCcsdsUart::framer.m_virtualFrameCount
  # For the comStatusIn breakpoints `condition` is the Fw::Success& being
  # forwarded -- SUCCESS=1, FAILURE=0.  A FAILURE arriving at ComQueue leaves it
  # WAITING (ComQueue.cpp:246) and is just as fatal as no status at all.
  printf "  args at this stop:\n"
  info args
  bt 8
end

echo \n================ stop limit reached; detaching ================\n
detach
