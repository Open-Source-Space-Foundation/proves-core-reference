# S-Band Reintegration TDD Plan

**Goal:** Bring the S-Band (SX1280/RadioLib) radio back into `main`, fixing the defects
that got it removed in PR #256 ("Remove SBand to protect RAM"), with every fix pinned
by a test written first.

**Related:** issue #122 (library can lock up / RAM concerns), issue #299 (stack
overflow analysis), PR #175 (original MVP), PR #256 (removal), branch
`s-band-speedup` (post-removal correctness fixes).

> Correction (found during PR 2): issue #109's radio fault management
> (RESET_RADIO + auto-reset) was never merged to main — it lives only on the
> unmerged `radio-sensor-fault-manage` branch (2273dc3). PR 2 built the fault
> surface fresh per D3; that branch remains reference material for PR 3 bench
> work only.

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
| D3 | Failure semantics: bounded call → N consecutive errors/timeouts → auto nRST reset → M failed resets → **FAULTED, latched until ground command**. Invariant: S-Band failure degrades to "no S-Band", never to backpressure, hang, or spacecraft reset. UHF unaffected. |
| D4 | Test seam: new `SBandRadioIf` abstract interface over the 13 SX1280 calls the component uses; production impl wraps SX1280 and owns the bounded-timeout logic; F´ UTs drive the component against a scripted fake. |
| D5 | Three staged PRs (infra → component+UTs → topology re-enable). PR 3 gated on ≥24 h dual-radio HWIL soak + end-to-end functional pass. |
| D6 | Program-wide `StackMonitor` component (all threads, 1 Hz, `k_thread_foreach` + `k_thread_stack_space_get`, needs `CONFIG_THREAD_MONITOR=y`). |
| D7 | Out of scope: throughput tuning (passthrough chunk/cooldown), async com-chain rework (documented fallback only), static stack-analysis tooling, stale `s-band-*` branch cleanup, fprime-zephyr changes (already merged via #326). |

**Fallback trigger:** if soak shows SBand stack high-water > 70 % of 8 KB, revisit
option C from #299 (async hop in the com chain) before flying.

---

## PR 1 — Infrastructure (SBand stays commented out; zero flight-behavior change)

### Slice 1.1 — StackMonitor component (tracer bullet)
New passive component `Components/StackMonitor`, seam: `ThreadInfoProviderIf`
(Zephyr impl trivial; UTs use a fake). One RED→GREEN cycle per behavior, in order:

1. On each `run` tick, reports minimum-free-stack for every thread the provider
   exposes (telemetry observable via the component tester).
2. Emits a WARNING_HI EVR when any thread's free stack drops below a threshold
   (percentage of its own size), throttled; clears on recovery.
3. Handles thread count changing between ticks without asserting.

### Slice 1.2 — Loud boot failure on task-start error
**RESOLVED 2026-07-13 (investigated):** `ActiveComponentBase::start()` already
FW_ASSERTs on failure, but the assert hook (`AssertFatalAdapter`) logs a FATAL
event and *continues*; the reboot arrives indirectly via FatalHandler →
stop-watchdog-feed → external WDT starvation. For SBand (started late, event
machinery alive) this path is loud. Resolution:
- No change to global assert semantics (FATAL→watchdog-starve was chosen
  deliberately in PR #259).
- fprime-zephyr PR (branch `fix/task-start-stack-alloc-logging`, 015c34a):
  `ZephyrTask::start()` logs task name + requested size on stack-alloc failure
  (compile-verified against this project). Submodule bump after upstream merge.
- PR 3 HWIL fault injection includes: impossible sband stack size → expect
  FATAL EVR + watchdog reset, not a silent limp.
- Known pre-existing gap, filed separately (out of scope): if one of the *first*
  components (`cmdDisp`, `events`) fails task-start, the FATAL is queued but
  never dispatched → genuine half-alive boot.

### Slice 1.3 — Kconfig flips
`CONFIG_DYNAMIC_THREAD_ALLOC=y`, `CONFIG_THREAD_MONITOR=y`. No new tests (config
only); gate = full existing suite + CI build for v5c/v5d/v5e + a short UHF bench
sanity to prove no regression while SBand is still off.

**PR 1 exit:** main builds and soaks exactly as before, now with per-thread stack
telemetry visible in YAMCS.

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
N consecutive failures → exactly one nRST reset request via the nRST GPIO, EVR,
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

> **BUILT 2026-07-13 (branch `feat/sband-pr3-topology`).** Two corrections to
> the paragraph above, found during implementation:
>
> - **CI does not actually matrix-build multiple boards today.** `.github/workflows/ci.yaml`
>   runs a single `make generate && make build`, and the board is whatever
>   `settings.ini`'s `BOARD=` line says (`proves_flight_control_board_v5e/rp2350a/m33`).
>   There is no per-board CI job for v5c/v5d/v5(base). This PR does not add one
>   (out of scope per D7 / "no existing pattern" guidance below) — v5d was
>   build-verified locally by swapping `settings.ini` and rerunning `make
>   generate && make build`, then reverting.
> - **No per-board FPP/CMake conditionalization mechanism exists in this repo**
>   (topology.fpp/instances.fpp compile identically for every board; the one
>   `cameraHandler`/"NOT ALL SATS HAVE CAMERAS" comment in
>   `ReferenceDeploymentTopology.cpp` is documentation, not a real `#if`
>   guard). So "un-comment the topology" is necessarily a **global** change —
>   every board that builds this topology now instantiates `sband` and must
>   resolve its devicetree nodes at compile time, or the build fails outright.
>
> **Board hardware findings (from the checked-in device trees, not assumed):**
> `boards/bronco_space/proves_flight_control_board_v5/proves_flight_control_board_v5.dtsi`
> had a `sband_nrst`/`sband_rx_en`/`sband_tx_en` block dead-commented (C-style
> comment) since before this effort, wired to raw GPIOs 17/21/22. Checking
> every board variant's actual pin usage (not just the comments) found:
> - `gpio0 17` (nrst): unused anywhere else on any board variant. Safe to revive.
> - `gpio0 21`/`22` (rx_en/tx_en): **conflict on v5e only** — v5e's SX126x LoRa
>   module (`&lora0` override in
>   `proves_flight_control_board_v5e_rp2350a_m33.dts`) already claims
>   `tx-enable-gpios`/`rx-enable-gpios` on those exact pins. This is why v5e's
>   hardware revision moved S-Band's RX/TX enables onto the MCP23017 expander
>   instead (`sband_rx_en`/`sband_tx_en` on `mcp23017` pins 4/7, already present
>   in v5e's own `.dts` before this PR). v5, v5c, and v5d all use the base
>   (SX1276) `&lora0` config, which claims neither pin — no conflict there.
> - IRQ/BUSY: the 4-pin generic `RF2_IO0..3` header on the MCP (present,
>   unmodified, on every board) had 2 pins repurposed for RX/TX-enable on v5e
>   only, leaving `rf2_io0`/`rf2_io1` free and identical across all four board
>   variants. This deployment's C++ (`ReferenceDeploymentTopology.cpp`) binds
>   `rf2_io1` → IRQ (carrying forward this draft's pre-existing choice) and
>   `rf2_io0` → BUSY (new for PR 3, since `getBusyLine` is a PR 2 addition).
>   **The BUSY pin assignment is inferred from the schematic's spare-pin count,
>   not confirmed against a wiring diagram — bench-verify in Slice 3.2.**
>
> **Resolution:** uncommented `sband_nrst`/`sband_rx_en`/`sband_tx_en` in the
> shared `v5.dtsi` (not per-board). Because devicetree node-path merging is
> "last property wins" — the same mechanism v5e's own `.dts` already relies on
> to override the inherited `&lora0` node — v5e's pre-existing MCP-based
> `sband_rx_en`/`sband_tx_en` declarations transparently take precedence over
> the newly-uncommented raw-GPIO ones, while `sband_nrst` (which v5e never
> overrides) is inherited from the shared block. This was **verified by an
> actual build**, not just reasoned about: `make generate && make build`
> succeeded for both v5e (mandatory) and v5d (attempted), with no devicetree
> duplicate-node or missing-nodelabel errors on either. **v5 (base) and v5c
> were not build-tested** (outside this task's verification gates) but are
> expected to build cleanly by the same reasoning — they inherit the same
> uncommented block unmodified and have no lora/pin conflicts.
>
> No board needed to be scoped out. Had a genuine per-board hardware gap
> existed (e.g. a board truly missing a required GPIO with no safe fallback),
> the right move would have been to stop and present options (DT stub aliases
> vs. a first-ever `CONFIG_BOARD_*`-gated C++ guard vs. accepting that board's
> breakage) rather than invent per-board FPP conditionalization that has no
> precedent in this codebase — that fork did not materialize here.

### Slice 3.2 — HWIL functional pass (bench, v5e)
- Downlink: TM frames received over the S-Band link end-to-end.
- Uplink: command through `ComCcsdsSband.authenticationRouter` → `cmdDisp` executes.
- StackMonitor shows the sband thread running with an 8 KB stack.

### Slice 3.3 — HWIL fault injection (the #122 repro, now with an expected answer)
Physically disconnect SPI/BUSY mid-operation → expect: bounded EVRs → auto-reset
attempts → FAULTED. **No watchdog reset, no queue overflow, UHF link still up.**
Then `RESET_RADIO` with hardware restored → link recovers.

### Slice 3.4 — Soak gate (merge gate for PR 3)
≥24 h continuous, both radios active with periodic S-Band TX/RX traffic:
- zero watchdog/unexpected resets;
- sband stack high-water < 70 % of 8 KB (else trigger the option-C fallback review);
- no other thread's watermark regresses vs the PR 1 baseline;
- heap watermark flat after init (STATIC_ONLY doing its job);
- RX/TX counters advance the whole window.

**Post-merge:** update #122 and #299 with results (and the RadioLib naming
correction); file the follow-up issue for throughput tuning (deferred
`s-band-speedup` perf commits + passthrough work).
