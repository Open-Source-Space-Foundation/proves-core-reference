# Patches Directory

This directory contains patches that are automatically applied to git submodules during the build process.

## fprime-gds-version.patch

This patch updates the `fprime-gds` version requirement in `lib/fprime/requirements.txt` from 4.1.0 to 4.1.1a2.

**Why:** The project requires fprime-gds 4.1.1a2 for specific features:
- file-uplink-cooldown argument
- file-uplink-chunk-size argument

The patch is automatically applied by the `make submodules` target to ensure version consistency and eliminate the version mismatch warning.

**Application:** This patch is applied automatically when running `make submodules` (or `make` which includes that target).

**Note:** After applying this patch, `git status` will show `lib/fprime` as modified. This is expected and should **not** be committed. The patched state is reapplied automatically on each `make submodules` run.

## fprime-com-aggregator-bounded-timeout.patch

Fixes issue #432: `Svc::ComAggregator`'s 10 Hz timeout signal FW_ASSERTs (queue FULL) whenever the component's dispatch thread stalls for longer than `queue_depth / timeout_rate` (~1.5 s at depth 15 / 10 Hz).

Upstream's `m_allow_timeout` guard (fprime #4402) only suppresses timeout signals in the WAIT_STATUS state. While the state machine sits in FILL, a stalled dispatch thread (downstream backpressure, thread starvation from CDC-ACM host stalls) still lets rate-group ticks fill the queue and trip the autocoded assert in `aggregationMachine_sendSignalFinish`.

The patch bounds timeout-signal queue occupancy in the hand-coded `timeout_handler`: the signal is only enqueued when the queue retains headroom for it plus the (flow-controlled, at most one each) in-flight `fill` and `status` signals. Timeout ticks are periodic and idempotent, so a skipped tick is retried on the next cycle — behavior is unchanged except that the queue can no longer overflow.

**Application:** Applied automatically by `make submodules`, same mechanism as the fprime-gds version patch. Candidate for upstreaming to nasa/fprime.
