# Components::StackMonitor

Program-wide stack-usage watchdog. Once per tick, walks every live Zephyr
thread (`k_thread_foreach`) and reports per-thread stack high-water telemetry,
warning when any thread's free stack drops below a configurable percent of its
own size and clearing on recovery.

Part of the S-Band reintegration plan, PR 1 / Slice 1.1 (decision D6): a
prerequisite for safely re-enabling the S-Band radio thread with a larger,
dedicated stack, by making per-thread stack pressure observable in YAMCS.

The stack-usage decision logic lives in `StackMonitorCore`, a pure C++ class
with no Zephyr or F Prime dependencies, so it is host-testable in isolation
(see `test/unit-tests/test_StackMonitor_Core.cpp`). `StackMonitor.cpp` is a
thin adapter: it walks Zephyr threads, builds a sample list, and feeds it to
the core each tick.

All storage is fixed-capacity POD (32 threads x 32-char names) kept as
component member state: the `k_thread_foreach` callback runs under the kernel
thread-monitor spinlock with IRQs locked, so it performs only a bounds check,
a bounded string copy, and integer stores. Zero heap allocation anywhere in
the monitoring path after boot (per-tick heap churn is the defect class that
got the S-Band radio removed; see issue #122). If a tick sees more live
threads than capacity, the extras are dropped and the `SampleOverflow`
telemetry channel reports it (no silent truncation).

## Telemetry
| Name | Description |
|---|---|
| MinFreeBytes | Free bytes remaining on the thread under the most stack pressure this tick |
| WorstThread | Name of the thread under the most stack pressure this tick |
| ThreadsBelowThreshold | Count of threads currently below the warn threshold |
| SampleOverflow | True when a tick saw more live threads than the monitor's fixed capacity |

## Events
| Name | Description |
|---|---|
| StackLow | A thread's free stack dropped below its warn threshold |
| StackRecovered | A previously-low thread recovered above its warn threshold |

## Change Log
| Date | Description |
|---|---|
|---| Initial Draft |
