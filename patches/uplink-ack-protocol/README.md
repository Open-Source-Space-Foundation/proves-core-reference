# Uplink ACK protocol — ground-side patch (prototype checkpoint)

`gds-fprime-gds-4.1.1a2.patch` is a unified diff against the **installed
`fprime-gds==4.1.1a2` wheel** (not vendored source — it patches whatever gets
`pip install`ed into `fprime-venv`). Apply after `make fprime-venv` / before
running comm:

```sh
patch -p1 -d fprime-venv/lib/python3.13/site-packages < patches/uplink-ack-protocol/gds-fprime-gds-4.1.1a2.patch
```

or `git apply --directory=fprime-venv/lib/python3.13/site-packages/fprime_gds ...`
adjusted for the strip level, depending on tooling available.

## What it does

1. **`common/communication/updown.py`**: stops fabricating the uplink
   handshake locally (`Uplinker.get_handshake()` loopback removed). The
   GDS used to ack itself; now it waits for the flight-generated ack
   (see the paired flight-side change in `ComCcsdsUart/ComCcsds.fpp` +
   `ProvesRouter`).
2. **`common/files/uplinker.py`**: matches acks via a sliding 5-byte
   prefix (type+seq) against the flight's echoed bytes, since flight-side
   framing shifts alignment vs. the raw sent packet; retransmits the
   in-flight packet on ack timeout (idempotent — `FileUplink` writes by
   byte offset and dedups by sequence) instead of aborting the transfer.
3. **`common/distributor/distributor.py`**, **`common/transport.py`**,
   **`common/communication/adapters/uart.py`**: `[TRACE ...]` timestamped
   print probes at each hop (ZMQ recv, distributor dispatch, serial
   read/write) used to diagnose ground-vs-flight latency. Debug-only,
   harmless to leave in for a prototype checkpoint; strip before any real
   PR.

## Why this isn't a normal repo change

`fprime-venv` is a generated virtualenv (gitignored), so these edits live
in `site-packages` and vanish on a fresh `make fprime-venv`. This patch
is the only way this checkpoint's ground-side behavior is reproducible.
A real fix would upstream 1-2 (protocol changes) into `fprime-gds` proper;
3 (trace probes) never should be upstreamed, just re-applied ad hoc when
debugging.

## Paired flight-side changes (in this branch's regular commits)

- `PROVESFlightControllerReference/Components/ProvesRouter/` — ack
  stash/emit on file-packet buffer return, keyed by the FW_PACKET_HAND
  (0x00FE) descriptor already used for the ack.
- `PROVESFlightControllerReference/ComCcsdsUart/ComCcsds.fpp` — wires
  `provesRouter.handshakeOut` into the EVENTS com queue.
- `lib/fprime` submodule: `Svc/ComAggregator/ComAggregator.cpp` — acks
  (FW_PACKET_HAND APID) bypass the FILL-state accumulate-until-timeout
  wait, cutting ~76ms/cycle. `Svc/FileUplink/FileUplink.cpp` — trace
  probes only, no functional change.
