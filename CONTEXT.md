# CONTEXT — PROVES Flight Radio

Glossary for the flight-software radio domain. Terms here are canonical; use them in code, docs, commands, and telemetry names.

## Radio paths

- **USP Radio Path** — the radio stack built on Semtech's Unified Software Platform (USP). Applies to SX126x-class boards (FCB v5e onward). Multi-modulation capable.
- **Legacy Radio Path** — the existing loramac-node-backed Zephyr `drivers/lora` stack wrapped by the `Zephyr::LoRa` component. Applies to SX127x-class boards (FCB v5/v5c/v5d). LoRa modulation only. Kept buildable, not extended.
- **USP (Unified Software Platform)** — Semtech's radio software platform (radio drivers + RAL + radio access arbitration + LoRa Basics Modem). Not to be confused with a Zephyr-project product; `usp_zephyr` is its Zephyr integration module.
- **LBM (LoRa Basics Modem)** — Semtech's modem library bundled inside USP; its LoRaWAN stack is unused by PROVES (we fly raw CCSDS point-to-point).
- **RAL (Radio Abstraction Layer)** — USP's chip-agnostic radio API. The seam the flight component talks to (and the seam mocked in unit tests).

## Link configuration

- **Link Profile** — a complete, named radio configuration: modulation plus every parameter needed for two radios to interoperate (e.g. for GFSK: bitrate, deviation, BT, sync word, CRC, preamble). Identified by index into the Profile Table. Profiles are switched atomically; individual RF parameters are never commanded piecemeal in operations.
- **Profile Table** — the versioned, checked-in list of Link Profiles shared verbatim by flight and ground builds. Both ends must be built from the same table version for a profile index to mean the same thing.
- **TX Profile / RX Profile** — the Link Profile currently applied to the transmit and receive directions independently. The link is asymmetric by design (e.g. robust LoRa uplink, high-rate GFSK downlink).
- **Boot-Default Profile** — the profile each direction starts in at boot, and the profile RX Auto-Revert falls back to. Chosen for maximum link robustness, not throughput.
- **RX Auto-Revert** — safety mechanism: after an RX Profile change, if no valid frame is received within the commanded revert window, the RX Profile reverts to the Boot-Default Profile. Receiving a valid frame on the new profile confirms it.

## Modulations

- **CW (Continuous Wave)** — unmodulated carrier transmission for beacons, range testing, and RF debug. A test mode, not a Link Profile.
- **GFSK** — Gaussian FSK packet modulation; the high-throughput downlink option on the USP Radio Path. Capped at ≤75 kbps (with fdev = 25 kHz) by the Band Constraint.
- **Band Constraint** — IARU coordination limits PROVES UHF emissions to ≤125 kHz occupied bandwidth. Every Link Profile must satisfy it.
- **LR-FHSS** — long-range frequency-hopping modulation. SX126x can transmit but never receive it, so it is out of scope until gateway-grade or LR20xx receive hardware exists in the ground segment.

## Ground segment

- **GRC (Ground Radio Controller)** — the station-local Zephyr radio box (SX1262) that terminates the RF link. Must consume the same Profile Table as flight.
