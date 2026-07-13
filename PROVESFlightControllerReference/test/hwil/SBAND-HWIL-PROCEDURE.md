# S-Band HWIL Bench Procedures (Slices 3.2 – 3.4)

**Status: NOT RUN.** This document was authored alongside the PR 3 topology
re-enable (`feat/sband-pr3-topology`) in an environment with no S-Band
hardware, no bench rig, and no ability to power-cycle real flight boards.
Everything below is a step-by-step procedure for whoever runs the bench —
none of it has been executed. Slice 3.1 (topology wiring, this PR) was
verified by software build/dictionary checks only; see
`S-BAND-REINTEGRATION-PLAN.md`'s Slice 3.1 as-built note for what that
covered and what it explicitly did not.

Channel, event, and command names below are taken directly from the
generated `ReferenceDeploymentTopologyDictionary.json` for the v5e build (the
mandatory board per Slice 3.1) — grep the dictionary for the exact strings if
in doubt; do not rely on memory of F Prime naming conventions.

## Prerequisites

- v5e flight control board (mandatory bench target for these procedures; see
  the Slice 3.1 as-built note in `S-BAND-REINTEGRATION-PLAN.md` for why v5e is
  the board with a fully-resolved, non-conflicting S-Band GPIO map).
- A second UHF-side ground station link (LoRa) live and operating normally
  throughout every procedure below — the D3 invariant under test is that
  S-Band failures never take UHF down with them.
- GDS/YAMCS session with the v5e dictionary loaded, command uplink capability
  on both `ComCcsdsSband.authenticationRouter` and
  `ComCcsdsLora.authenticationRouter`.
- Bench access to the SPI0 bus (chip-select index 1, `gpio0 7` per
  `boards/bronco_space/proves_flight_control_board_v5/proves_flight_control_board_v5.dtsi`'s
  `&spi0` node) and the BUSY line, for the fault-injection procedure.
- Confirm firmware under test is this branch's build
  (`ReferenceDeployment.version` / `CdhCore.version` telemetry) and that
  `ReferenceDeployment.sband` appears live in the loaded dictionary (it will
  not, on any firmware image built before this PR).

## Open bench-only unknown to resolve before/during Slice 3.2

PR 3's software-only verification could not confirm the physical pin
assignment for the S-Band BUSY line. `ReferenceDeploymentTopology.cpp` binds
`SBand.getBusyLine` to devicetree node `rf2_io0` and `SBand.getIRQLine` to
`rf2_io1` — both are the two spare pins of the generic 4-pin `RF2_IO0..3`
header on the MCP23017 expander (see the plan doc's Slice 3.1 note for the
full reasoning). The IRQ assignment (`rf2_io1`) carries forward this
deployment's pre-existing draft choice; the BUSY assignment (`rf2_io0`) is
new for PR 3 and is an inference from "which pins are left," not a confirmed
schematic reading. **Before trusting BUSY-driven behavior in Slice 3.3,
verify with a meter/scope that toggling the SX1280's physical BUSY pin
changes `rf2_io0`'s logic level** (e.g. observe `SBand`'s behavior change, or
probe the MCP GPIO directly). If the mapping is backwards, swap the
`DT_NODELABEL` arguments in `ReferenceDeploymentTopology.cpp`'s
`sbandBusyGpio`/`sbandIrqGpio` declarations and re-run.

---

## Slice 3.2 — Functional pass (bench, v5e)

Goal: prove the full downlink and uplink data paths work end-to-end over a
real S-Band RF link, and that the sband thread is actually running with the
stack size PR 3 gave it.

### 3.2.1 — Boot and thread sanity

1. Flash the v5e build from this branch. Power on.
2. Confirm no FATAL boot event and `ReferenceDeployment.startupManager.BootCount`
   increments by exactly 1 from its last known value (no unexpected reboot
   during init).
3. Poll `ReferenceDeployment.stackMonitor.WorstThread` /
   `ReferenceDeployment.stackMonitor.MinFreeBytes` for several ticks. Because
   `StackMonitor` only reports the single worst thread per 1 Hz tick (see
   `Components/StackMonitor/StackMonitor.fpp`), the sband thread will not
   necessarily appear here unless it is the worst one — that's expected. To
   directly confirm the sband thread exists and was sized correctly, use a
   ground-side or serial-console dump of `k_thread_foreach` output (or add a
   temporary breakpoint/log) and confirm a thread whose name matches the
   `sband` F Prime instance has an 8192-byte stack (`Default.SBAND_STACK_SIZE`
   from `instances.fpp`), not the shared 4096-byte pool size other threads
   get.
4. Confirm no `Components::StackMonitor::StackLow` event fires for the sband
   thread during quiescent boot (a low-water event this early would indicate
   the 8 KB budget is already under-sized before any RX/TX traffic).

### 3.2.2 — Downlink (TM frames over S-Band)

1. From the ground station, confirm nominal signal reception on the S-Band
   frequency/modulation configured via `ReferenceDeployment.sband`'s
   parameters (`DATA_RATE`, `CODING_RATE`, `BANDWIDTH_TX`, `BANDWIDTH_RX` —
   see `Components/SBand/SBand.fpp`).
2. Command `ReferenceDeployment.sband.TRANSMIT(ENABLED)`. Confirm the command
   is accepted (`CdhCore.cmdDisp.CommandsDispatched` increments, no
   `CommandsDropped`).
3. Confirm the ground S-Band receiver captures valid CCSDS TM frames
   (physical/RF-layer proof of downlink; there is no onboard per-frame byte
   counter for S-Band specifically — `ComCcsdsSband`'s internal framer/
   frameAccumulator/apidManager components carry no telemetry of their own,
   unlike `lora.BytesSent`/`lora.BytesReceived` on the LoRa driver. Treat
   ground-side frame receipt as the primary evidence for this step).
4. Confirm `ReferenceDeployment.sband.LastRssi` / `LastSnr` are populating
   with plausible values (these update only "on change" — i.e. only when a
   packet round-trips through the radio, so watch for them changing over
   time, not just a single non-zero snapshot).

### 3.2.3 — Uplink (command through S-Band)

1. From the ground station, send a valid authenticated command frame over
   S-Band targeting any safe, observable command (e.g. a telemetry-only
   command, not `RESET_RADIO` yet).
2. Confirm `ComCcsdsSband.authenticatesband.AuthenticatedPacketsCount`
   increments and `ComCcsdsSband.authenticatesband.CurrentSequenceNumber`
   advances.
3. Confirm `ComCcsdsSband.authenticationRouter.PassedRouter` increments and
   the command actually executes (dispatched through
   `ComCcsdsSband.authenticationRouter.commandOut` → `CdhCore.cmdDisp` per
   the `ComCcsds_CdhCore` connections in `topology.fpp`) — verify via
   `CdhCore.cmdDisp.CommandsDispatched` and the command's own expected
   side-effect (e.g. a telemetry value change, an EVR).
4. Send one deliberately invalid/replayed frame; confirm
   `ComCcsdsSband.authenticatesband.RejectedPacketsCount` or
   `ComCcsdsSband.authenticationRouter.FailedRouter` increments instead of
   the command executing, and that no crash/reset results.

### 3.2.4 — Exit criteria

- Downlink TM confirmed on the ground receiver.
- At least one uplinked command executed end-to-end through
  `ComCcsdsSband.authenticationRouter` → `cmdDisp`.
- sband thread confirmed running with an 8 KB (not 4 KB) stack.
- No FATAL events, no unexpected `BootCount` increments, during the whole pass.

---

## Slice 3.3 — Fault injection (the #122 repro, now with an expected answer)

Goal: prove the D3 invariant (`S-BAND-REINTEGRATION-PLAN.md`) — a wedged
radio degrades to "no S-Band," never to a spacecraft reset, queue backpressure,
or a dead UHF link — and prove the ground-recovery path works.

### 3.3.1 — Baseline

1. With the board powered and S-Band operating nominally (per Slice 3.2),
   confirm `ReferenceDeployment.sband.RadioFaulted = false`,
   `ConsecutiveRadioFailures = 0`, `RadioResetCount` at its current baseline.
   confirm the UHF/LoRa link is also nominal (send/receive a command over
   `ComCcsdsLora.authenticationRouter` and confirm it executes) — this is the
   "before" baseline for the isolation check in 3.3.3.

### 3.3.2 — Inject the fault

1. Physically disconnect the SPI0 bus chip-select-1 line or the BUSY line to
   the SX1280 (see Prerequisites) while S-Band is mid-transmit or mid-receive.
2. Expect, in order:
   - A bounded run of `Components::SBand::RadioLibFailed` and/or
     `RadioNotConfigured` events (each individually throttled — see
     `SBand.fpp`'s `throttle 2`/`throttle 3` — so do not expect an unbounded
     flood).
   - After exactly `SBandFaultPolicy::CONSECUTIVE_FAILURE_LIMIT` (= 5, see
     `Components/SBand/SBandFaultPolicy.hpp`) consecutive failed radio
     operations: one `RadioResetRequested(consecutiveFailures=5)` event and
     `ReferenceDeployment.sband.ConsecutiveRadioFailures` telemetry reflecting
     the count at the moment of the request (policy resets the counter after
     the reset attempt completes).
   - This nRST-reset-and-retry cycle repeats. After
     `SBandFaultPolicy::RESET_ATTEMPT_LIMIT` (= 3) resets without an
     intervening successful operation: one `RadioFaultLatched(resetCount=3)`
     event, and `ReferenceDeployment.sband.RadioFaulted` telemetry flips to
     `true` and stays there (latched — no further reset attempts, per D3).
3. Confirm `ReferenceDeployment.sband.RadioResetCount` reflects the resets
   actually performed (bounded at 3 for this one fault episode, per the
   latch).

### 3.3.3 — Confirm isolation (the actual point of this test)

While `RadioFaulted = true`:

1. **No watchdog reset.** Confirm `ReferenceDeployment.watchdog.WatchdogTransitions`
   does not increment during or after the fault sequence, and
   `ReferenceDeployment.startupManager.BootCount` does not increment (i.e.
   the board never rebooted).
2. **No queue overflow / backpressure.** Confirm
   `ComCcsdsSband.comQueue.comQueueDepth` /
   `ComCcsdsSband.commsBufferManager.HiBuffs` /
   `ComCcsdsSband.commsBufferManager.NoBuffs` do not show unbounded growth —
   FAULTED means `SBand`'s `run`/`dataIn` calls become no-ops that still
   return buffers immediately (per Slice 2.7 in the plan doc), not a stall.
3. **UHF link still up.** Send a command over
   `ComCcsdsLora.authenticationRouter` and confirm it executes normally
   (`CdhCore.cmdDisp.CommandsDispatched` increments,
   `ComCcsdsLora.authenticationRouter.PassedRouter` increments) — this is the
   core D3 assertion: an S-Band hardware fault must be invisible to UHF.
4. Confirm `ReferenceDeployment.stackMonitor` shows no new `StackLow` event
   for any thread during the fault episode (a wedged SPI/BUSY line should not
   manifest as a stack problem; if it does, that is a new bug, not the
   expected fault path).

### 3.3.4 — Ground recovery

1. Physically restore the SPI0/BUSY connection.
2. Command `ReferenceDeployment.sband.RESET_RADIO()`.
3. Expect exactly one `Components::SBand::RadioFaultCleared` event,
   `ReferenceDeployment.sband.RadioFaulted` returns to `false`, and a
   subsequent radio operation (e.g. re-run Slice 3.2.2's downlink check)
   succeeds.
4. Confirm `ConsecutiveRadioFailures` reset to 0 after the successful
   recovery.

### 3.3.5 — Boot-failure case (Slice 1.2 resolution, verify here since it needs a real reboot)

This exercises the companion PR 1 finding (`S-BAND-REINTEGRATION-PLAN.md`
Slice 1.2): a stack-allocation failure at task-start time must be loud, not a
silent limp.

1. Build a **temporary, do-not-merge** variant of this firmware with
   `Default.SBAND_STACK_SIZE` in `instances.fpp` set to an impossible value
   (e.g. larger than `CONFIG_DYNAMIC_THREAD_POOL_SIZE * CONFIG_DYNAMIC_THREAD_STACK_SIZE`
   combined, or simply larger than available RAM — see `prj.conf`'s
   `CONFIG_DYNAMIC_THREAD_ALLOC`/`CONFIG_DYNAMIC_THREAD_POOL_SIZE` comments
   for the current budget).
2. Flash and boot this variant on the bench (not on a board anyone cares
   about recovering easily — expect a boot loop or a hung board if this goes
   wrong).
3. Expect: a FATAL event logged (task-start stack allocation failure,
   surfaced per the fprime-zephyr `ZephyrTask::start()` logging fix referenced
   in Slice 1.2), followed by a watchdog-driven reset (FatalHandler stops
   watchdog feed → external WDT starves → reset) — **not** a silent hang and
   **not** a partial boot with S-Band quietly missing.
4. Confirm `ReferenceDeployment.startupManager.BootCount` increments,
   confirming the reset actually happened.
5. Discard this temporary build; do not merge or flash it outside this one
   check.

### 3.3.6 — Exit criteria

- Fault sequence matches the expected EVR/telemetry sequence in 3.3.2 exactly
  (bounded EVRs → reset requests → latched fault, no more, no less).
- No watchdog reset, no queue overflow, UHF unaffected, during the fault.
- `RESET_RADIO` recovers the radio fully after hardware is restored.
- Impossible-stack-size boot variant produces a loud FATAL + reset, not a
  silent failure.

---

## Slice 3.4 — 24 h dual-radio soak (PR 3 merge gate)

Goal: the actual merge gate from `S-BAND-REINTEGRATION-PLAN.md`. Run this
last, only after 3.2 and 3.3 both pass cleanly.

### Setup

- Flash this branch's build (the real one, not the Slice 3.3.5 broken
  variant) on v5e.
- Both radios active: S-Band and UHF/LoRa both transmitting/receiving
  periodic traffic for the entire window (not just at the start/end).
- Baseline every gate metric below *before* starting the clock, using the PR
  1 baseline build (StackMonitor was introduced in PR 1; if a PR 1 baseline
  soak log is not already on hand, capture one run of comparable length on
  the current `main` — pre-PR-3 — build first so there's something to diff
  against).
- Duration: **≥ 24 hours continuous**, no manual intervention.

### Merge gates (all must hold for the full window)

1. **Zero unexpected resets.** `ReferenceDeployment.startupManager.BootCount`
   does not increment beyond any deliberately-commanded reboots (there
   should be none), and `ReferenceDeployment.watchdog.WatchdogTransitions`
   stays flat.
2. **sband stack high-water < 70 % of 8 KB.** Because `StackMonitor` only
   reports the single worst thread per tick
   (`ReferenceDeployment.stackMonitor.WorstThread` /
   `MinFreeBytes`), do not rely on it alone to find the sband thread's own
   number if some other thread is worse that tick — capture the full
   `k_thread_foreach` dump periodically (or add a temporary per-thread dump
   command) and specifically track the sband thread's minimum free-bytes
   over the whole window. 70 % of 8192 bytes = 5734 bytes used is the
   trigger threshold (i.e. free bytes must stay above 2458 bytes,
   `8192 * 0.30`). If this trips, **do not merge** — re-open the option-C
   fallback review referenced in the plan doc's "Fallback trigger" note
   instead.
3. **No other thread's watermark regresses vs. the PR 1 baseline.** Diff
   every thread's minimum free-bytes over the run against the PR 1 baseline
   capture from Setup above. sband itself is exempt (it didn't exist in the
   PR 1 baseline); every other thread should show no meaningful regression —
   a regression here would mean sband's presence (extra RAM pressure,
   scheduling contention) is hurting unrelated threads.
4. **Heap watermark flat after init.** Confirm no heap-growth trend for the
   duration of the run — this is meant to prove `RADIOLIB_STATIC_ONLY=1`
   (PR 2, Slice 2.10) is actually preventing in-flight RadioLib heap churn.
   If the board exposes a heap-usage telemetry/debug hook, sample it
   periodically; at minimum, confirm no `Fw::MallocAllocator`-related
   failures/EVRs appear and no `AllocationFailed` events fire on `sband`
   over the window.
5. **RX/TX counters advance the whole window.** As noted in Slice 3.2.2,
   there is no onboard per-frame byte/packet counter for S-Band specifically.
   Use `ComCcsdsSband.authenticatesband.CurrentSequenceNumber` /
   `AuthenticatedPacketsCount` (uplink) advancing steadily, plus ground-side
   confirmation that downlink TM frames keep arriving throughout the 24 h
   (not just at the start), as the two lines of evidence for this gate. If
   this gap (no onboard TX/RX counters for S-Band) is judged worth closing,
   file it as a follow-up rather than blocking this soak on adding new
   telemetry.

### Exit criteria

All five gates hold for the full ≥24 h window with zero manual intervention.
On pass: merge PR 3, then follow `S-BAND-REINTEGRATION-PLAN.md`'s
"Post-merge" step (update #122/#299, file the throughput-tuning follow-up).
On any gate failure: do not merge; capture the failure telemetry/EVR log,
open a new issue with the specific gate and observed values, and revisit
before re-attempting the soak.
