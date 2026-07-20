# S-Band Reintegration TDD Plan

**Goal:** Bring the S-Band (SX1280/RadioLib) radio back into `main`, fixing the defects
that got it removed in PR #256 ("Remove SBand to protect RAM"), with every fix pinned
by a test written first.

**Related:** issue #122 (library can lock up / RAM concerns), issue #299 (stack
overflow analysis), PR #175 (original MVP), PR #256 (removal), PR #109 (radio fault
management / nRST reset), branch `s-band-speedup` (post-removal correctness fixes).

> Naming note: #122 says "RadioHead" but the flight code uses **RadioLib**
> (`lib/RadioLib`, jgromes). Correct this when updating the issues.

---

## Root causes (as-investigated, 2026-07-13)

1. **Stack overflow on RX** (#299): `deferredRxHandler` runs on the SBand thread with a
   256-byte local array, then calls `dataOut_out` — a *synchronous* call chain through
   `ComCcsdsSband` (frameAccumulator → deframer → router) — overflowing the 4096-byte
   stack.
2. **Why "just raise the stack" failed:** `CONFIG_DYNAMIC_THREAD_STACK_SIZE=4096` is a
   single global for the 25-slot Zephyr thread pool (`DYNAMIC_THREAD_ALLOC=n`). Raising
   it costs ~100 KB across all threads on a 520 KB part → no boot.
   **Never tried:** `CONFIG_DYNAMIC_THREAD_ALLOC=y` lets one oversized request fall back
   to a single boot-time `k_malloc` (~8 KB total cost); all other threads keep pool stacks.
3. **In-flight heap churn** (#122 comment): RadioLib defaults to dynamic allocation;
   `RADIOLIB_STATIC_ONLY` is not set.
4. **Unbounded waits** (#122): RadioLib internal BUSY/status polling can spin forever if
   SPI/BUSY wedges (the BUSY line wasn't even wired until `s-band-speedup` 7c473f4).
5. **No tests:** the SBand UT block is commented out; the component has no seam
   (`FprimeHal` → `Module` → `SX1280` held by value).

## Decisions (grilled 2026-07-12/13)

| # | Decision |
|---|----------|
| D1 | Resurrect the existing RadioLib driver in place (no Zephyr-native rewrite, no com-chain async rework). Cherry-pick only *correctness* commits from `s-band-speedup`. |
| D2 | SBand thread gets a dedicated 8 KB stack via `CONFIG_DYNAMIC_THREAD_ALLOC=y` fallback; boot-failure is loud (assert/FATAL); stack usage is *measured*, not assumed. |
| D3 | Failure semantics: bounded call → N consecutive errors/timeouts → auto nRST reset (reuse #109 path) → M failed resets → **FAULTED, latched until ground command**. Invariant: S-Band failure degrades to "no S-Band", never to backpressure, hang, or spacecraft reset. UHF unaffected. |
| D4 | Test seam: new `SBandRadioIf` abstract interface over the 13 SX1280 calls the component uses; production impl wraps SX1280 and owns the bounded-timeout logic; F´ UTs drive the component against a scripted fake. |
| D5 | Three staged PRs (infra → component+UTs → topology re-enable). PR 3 gated on ≥24 h dual-radio HWIL soak + end-to-end functional pass. |
| D6 | Program-wide `StackMonitor` component (all threads, 1 Hz, `k_thread_foreach_unlocked` + `k_thread_stack_space_get`, needs `CONFIG_THREAD_MONITOR=y` and `CONFIG_INIT_STACKS=y`). |
| D7 | Out of scope: throughput tuning (passthrough chunk/cooldown), async com-chain rework (documented fallback only), static stack-analysis tooling, stale `s-band-*` branch cleanup, fprime-zephyr changes (already merged via #326). |

**Fallback trigger:** if soak shows SBand stack high-water > 70 % of 8 KB, revisit
option C from #299 (async hop in the com chain) before flying.

---

## PR 1 — Infrastructure (SBand stays commented out; zero flight-behavior change)

### Slice 1.1 — StackMonitor component (tracer bullet)

New passive component `Components/StackMonitor`. No provider seam: the Zephyr
adapter (`StackMonitor.cpp`) calls `k_thread_foreach_unlocked` +
`k_thread_stack_space_get` directly and stays a thin, heap-free translation
into fixed-capacity samples; all decision logic (summary, warning/recovery
transitions, overflow) lives in the pure, host-testable `StackMonitorCore`.
One RED→GREEN cycle per behavior in `StackMonitorCore`, in order:

1. On each `tick`, computes the worst thread's free bytes (`MinFreeBytes`/
   `WorstThread`), the count of threads below the warning threshold, and
   whether the fixed-capacity sample buffer overflowed — this is what
   `StackMonitor` publishes as telemetry.
2. Emits a WARNING_HI EVR when any thread's free stack drops below a threshold
   (percentage of its own size), throttled; clears on recovery.
3. Handles thread count changing between ticks without asserting.

The Zephyr adapter itself (the `k_thread_foreach_unlocked` callback, real
telemetry/EVR wiring) has no host build target and is validated via
integration/HWIL in PR 3, not component-level UTs.

### Slice 1.2 — Loud boot failure on task-start error

Verify what `ActiveComponentBase`/`Os::Task` does today when
`k_thread_stack_alloc` returns null (`ERROR_RESOURCES`). If it can silently limp,
add an assert/FATAL. Behavior to pin (host UT where the Os layer permits, else
HWIL check in PR 3): a deployment whose thread can't get its stack never runs
half-alive.

### Slice 1.3 — Kconfig flips

`CONFIG_DYNAMIC_THREAD_ALLOC=y`, `CONFIG_THREAD_MONITOR=y`, `CONFIG_INIT_STACKS=y`
(required for `k_thread_stack_space_get`). No new tests (config only); gate =
full existing suite + CI build for v5c/v5d/v5e + a short UHF bench sanity to
prove no regression while SBand is still off.

**PR 1 exit:** main builds and soaks exactly as before, now with worst-thread
stack telemetry (`MinFreeBytes`/`WorstThread`/`ThreadsBelowThreshold`/
`SampleOverflow`) visible in YAMCS's Health packet.

---

## PR 2 — Component rework + UTs (built, tested, still not in the topology)

Un-comment `add_fprime_subdirectory(.../SBand/)` and the UT registration; the
library and its tests build without any topology change.

### Slice 2.1 — Seam + first light (tracer bullet)

RED: component UT constructs SBand with a fake `SBandRadioIf`; asserts that startup
configures the radio and arms receive. GREEN: introduce `SBandRadioIf`, refactor
SBand to hold a reference (production default = RadioLib-backed impl), pass.
This cycle proves the whole UT harness path works.

### Slice 2.2 — RX happy path

Fake presents a received packet → component emits `dataOut` with the exact bytes,
re-arms receive, updates RSSI/SNR telemetry. (Implementation note, not test
subject: read directly into the allocated `Fw::Buffer` — the 256-byte stack array
goes away here.)

### Slice 2.3 — RX allocation failure

Buffer manager returns invalid buffer → WARNING EVR, receive re-armed, no leak,
no crash.

### Slice 2.4 — TX path + transmit gate

`dataIn` while ENABLED → radio transmit called with the frame, buffer returned,
comStatus emitted. While DISABLED → no radio call, buffer still returned,
comStatus still emitted (the com queue must never starve).

### Slice 2.5 — Errors are bounded and counted

Fake returns an error / simulated timeout → call completes promptly, EVR emitted,
consecutive-error counter advances; one success resets the counter.

### Slice 2.6 — Auto-reset escalation

N consecutive failures → exactly one nRST reset request via the #109 path, EVR,
counters observable in telemetry.

### Slice 2.7 — FAULTED latch

M resets without recovery → FAULTED: telemetry flag set; no further radio-interface
calls; `dataIn` buffers returned immediately; `run` ticks are no-ops. Latched —
further errors can't re-trigger resets.

### Slice 2.8 — Ground recovery

`RESET_RADIO` (existing command) while FAULTED → full re-init through the fake,
FAULTED cleared on success, re-latched if re-init fails.

### Slice 2.9 — Cherry-pick `s-band-speedup` correctness fixes

BUSY-GPIO wiring into RadioLib (7c473f6/363101e), `dataOut_out` after all SPI ops
(8aeee2f), atomic inter-thread flag (9b240d0), enableRx/enableTx return types
(17df3ec). Where a fix has observable behavior (ordering, flag races), pin it with
a UT first; pure wiring is covered by PR 3 bench work.

### Slice 2.10 — `RADIOLIB_STATIC_ONLY=1`

Set in SBand's CMake (where RadioLib is added). Gate = build + full UT suite;
grep the map file to confirm no RadioLib heap symbols remain on hot paths.

**PR 2 exit:** SBand component fully unit-tested against the D3 invariant; zero
topology/flight change.

---

## PR 3 — Topology re-enable (soak-gated)

### Slice 3.1 — Wire it back

Un-comment instances, `ComCcsdsSband` subtopology, connections, rate-group slots.
New `SBAND_STACK_SIZE = 8 * 1024` for the sband instance only (comment explaining
the pool-fallback mechanism). CI builds for v5c/v5d/v5e; existing integration +
day-in-the-life suites green.

### Slice 3.2 — HWIL functional pass (bench, v5e)

- Downlink: TM frames received over the S-Band link end-to-end.
- Uplink: command through `ComCcsdsSband.authenticationRouter` → `cmdDisp` executes.
- HWIL console/debug view (not telemetry — StackMonitor only reports the
  worst thread) confirms the sband thread is running with an 8 KB stack.

### Slice 3.3 — HWIL fault injection (the #122 repro, now with an expected answer)

Physically disconnect SPI/BUSY mid-operation → expect: bounded EVRs → auto-reset
attempts → FAULTED. **No watchdog reset, no queue overflow, UHF link still up.**
Then `RESET_RADIO` with hardware restored → link recovers.

### Slice 3.4 — Soak gate (merge gate for PR 3)

≥24 h continuous, both radios active with periodic S-Band TX/RX traffic:
- zero watchdog/unexpected resets;
- sband stack high-water < 70 % of 8 KB, observed via HWIL console/debug (not
  StackMonitor telemetry, which only reports the worst thread) — else trigger
  the option-C fallback review;
- StackMonitor's worst-thread watermark doesn't regress vs. the PR 1 baseline,
  cross-checked against the HWIL per-thread console view for the sband thread
  specifically;
- heap watermark flat after init (STATIC_ONLY doing its job);
- RX/TX counters advance the whole window.

**Post-merge:** update #122 and #299 with results (and the RadioLib naming
correction); file the follow-up issue for throughput tuning (deferred
`s-band-speedup` perf commits + passthrough work).
