# 0001 — Adopt Semtech USP (RAL layer) for the SX126x radio path; retain loramac-node for SX127x

Date: 2026-07-04
Status: Accepted

## Context

The UHF radio component (`Zephyr::LoRa` in lib/fprime-zephyr) wraps Zephyr's classic
`drivers/lora` API backed by loramac-node. That API can only express LoRa modulation,
loramac-node is in maintenance mode upstream, and the project wants GFSK (high-rate
downlink) and a clean CW path.

Three integration options existed:

1. **Semtech `usp_zephyr` module, component talks to RAL/RAC directly.** Full
   modulation menu (LoRa/GFSK/LR-FHSS/CW), future LR20xx path. Supports SX126x/LR11xx/
   LR20xx only — no SX127x, no SX128x. Validated on Zephyr 4.2 (we run 4.3).
2. **In-tree Zephyr 4.3 LBM backend** (`drivers/lora/lora_basics_modem`, needs the
   `lora-basics-modem` west module). Zero component change, covers SX126x *and* SX127x —
   but sits behind the standard `lora_modem_config` API, so no new modulations.
3. **Two-phase**: option 2 first for all boards, option 1 later. Touches every board
   twice, roughly doubles schedule.

Hardware reality: FCB v5/v5c/v5d carry SX1276 (SX127x — unsupported by USP forever;
Semtech has ended new SX127x software). FCB v5e carries SX1262. The S-band component
(SX1280 via RadioLib) is unsupported by USP until LR2021-class hardware exists.

## Decision

- New F´ component (`Zephyr::UspRadio`, sibling of `Zephyr::LoRa` in lib/fprime-zephyr)
  targets **USP's RAL directly** via the `usp_zephyr` west module, bypassing the standard
  Zephyr `drivers/lora` API.
- **SX127x boards keep the Legacy Radio Path unchanged** (`Zephyr::LoRa` + loramac-node).
  Board/topology config selects which component is instantiated; no ifdef sharing of one
  implementation.
- The new component keeps the existing Svc.Com + Svc.BufferAllocation port surface and
  the `TRANSMIT` / `CONTINUOUS_WAVE` command names and arguments verbatim, adding
  profile commands on top (see ADR 0002).
- S-band stays on RadioLib; LR2021 is noted as the future convergence path.
- Component is **active (queued)** using the SBand deferred-handler pattern: all
  RAL/SPI work runs on the component thread, callbacks only enqueue. (The SX126x
  post-sleep first-SPI-command drop we diagnosed is the class of bug this prevents.)

## Consequences

- Modulation features land only on v5e+; v5c/v5d get no new radio capability, ever.
- Two radio components coexist in lib/fprime-zephyr indefinitely; ground dictionaries
  differ per board revision.
- We take on Zephyr 4.3-vs-4.2 validation risk for usp_zephyr ourselves (Phase 0 spike
  gates the plan).
- Devicetree must ensure exactly one driver binds the `semtech,sx1262` node when both
  the in-tree Zephyr driver and USP's driver are present in the tree.
- The standard Zephyr LoRa shell/API tooling does not see the USP radio; all operations
  go through F´ commands.
