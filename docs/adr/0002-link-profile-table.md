# 0002 — Versioned Link Profile table, split TX/RX selection, RX auto-revert

Date: 2026-07-04
Status: Accepted

## Context

GFSK introduces many coupled RF parameters (bitrate, deviation, BT, sync word, CRC,
preamble). A single mismatched field between spacecraft and ground silently kills the
link, and a bad RX configuration on the spacecraft strands it (deaf to commands).
Free-form per-parameter F´ params (the current `CODING_RATE`/`DATA_RATE`/`BANDWIDTH_*`
approach, extended) would make mismatches easy and atomic switches impossible.

The dominant operational use case is asymmetric: keep the uplink (spacecraft RX) on
robust LoRa while switching only the downlink (spacecraft TX) to GFSK for bulk data.
The existing component already has separate TX/RX bandwidth params — the asymmetry
precedent exists.

## Decision

- Radio configuration is selected only by **Link Profile index** into a **versioned
  Profile Table** checked into one shared artifact consumed by both the flight build
  and the GRC build. The table version is downlinked as telemetry.
- Selection is **per direction**: `SET_TX_PROFILE(idx)` and `SET_RX_PROFILE(idx,
  revert_s)`.
- `SET_TX_PROFILE` is unguarded — worst case is a lost downlink until the next command.
- `SET_RX_PROFILE` is guarded by **RX Auto-Revert**: if no valid frame is received
  within `revert_s`, RX reverts to the Boot-Default Profile. A valid frame received on
  the new profile confirms it. There is no separate confirm command.
- Individual RF parameters are not commandable in operations; effective parameters are
  visible read-only via telemetry. (A lab-only raw-config path was considered and
  deferred — bench experiments can rebuild the table instead.)

## Consequences

- Adding or changing a profile is a coordinated flight+ground release (table version
  bump), not an on-orbit parameter tweak. This is deliberate friction.
- The uplink can be experimented with safely; a failed RX experiment self-heals within
  the revert window.
- The legacy `Zephyr::LoRa` params remain only on SX127x boards; the new component does
  not carry them.
- Profile indices become part of ops vocabulary and sequences; renumbering existing
  entries is forbidden (append-only table).
