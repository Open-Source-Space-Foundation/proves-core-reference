# Investigation: integration tests green on the local bench and in CI

**Goal: `provision_key_test.py` and the rest of the integration suite pass both
on the local bench and in CI, on a board that starts keyless.**

The firmware-side blockers are fixed and verified on hardware (see commit
`ec0bdb37`; the previous edition of this file, which chased a boot "hang" that
turned out not to exist, is in git history and is superseded). What remains is
getting the *test path* to work, plus one design decision the provisioning flow
forces.

Two open items:

1. `provision_key_test.py` errors in pytest setup on the bench — **cause not yet
   established**.
2. A mis-provisioned key cannot be recovered from the ground — **needs a
   deliberate decision**.

---

## Issue 1 — `provision_key_test.py` never reaches its test body

### Status: unconfirmed. Firmware side proven; test/GDS side not.

`make test-integration FILTER=provision_key` fails with
`ERROR ... test_provision_key` — an error in *setup*, not an assertion failure.
The test body never runs.

### What is proven

- **The firmware provisioning path works.** Sending the identical command
  outside pytest —
  `fprime-cli command-send ComCcsdsUart.tcSecurityDeframer.PROVISION_KEY
  --arguments 0 <32 hex chars>` — provisions the board: the key store went to
  `valid=1 spi=0` with the expected bytes, survived a cold reboot, and
  subsequently authenticated an uplink `SET_SEQ_NUM`.
- **The bypass allowlist works on a keyless board.** `CMD_NO_OP`
  (`0x01000000`) and `PROVISION_KEY` (`0x2100B002` / `0x2200B002`) are all in
  `kBypassOpCodes` (`Bypasser.cpp`), and the opcodes match a freshly generated
  dictionary.
- **Commands were reaching the dispatcher.** After a bench session containing
  several `CMD_NO_OP` attempts, `ComCcsdsUart::provesRouter` read
  `routed=3 bypassed=3 rejected=0` over SWD — some uplink commands were accepted
  and dispatched with no key present, and **nothing** was rejected.
- **The board was healthy and transmitting** during the failing runs: telemetry
  flowing on the CDC, and GDS's `comm.py.log` showing deframed downlink.

### What fails

`start_gds` (`test/int/conftest.py:113`, session-scoped) loops for 30 s doing
`send_and_assert_command("CdhCore.cmdDisp.CMD_NO_OP")`, which waits on a
two-item event sequence (`OpCodeDispatched` + `OpCodeCompleted`). That sequence
search times out on every attempt.

Because `recover_from_safe_mode` (`conftest.py:177`) is `autouse=True` **and**
depends on `start_gds`, that fixture runs for *every* test in
`PROVESFlightControllerReference/test/int/` — so when it fails, the whole
directory errors in setup, including `provision_key_test.py`.

### Leading hypothesis (not yet tested)

A GDS-side downlink desync caused by the bench workflow rather than a product
defect. After every board reset during the session GDS logged

```
[WARNING] framing: APID 2 received sequence count: 4 (expected: 1)
```

i.e. its deframer's expected space-packet sequence count was stale relative to a
board that had rebooted underneath a long-lived GDS process. If GDS is
discarding out-of-sequence frames, the command *is* dispatched on the board (the
router counters agree) but the responding events never reach the test API — an
exact match for the observed symptom.

CI does not have this exposure: `.github/actions/flash-firmware` power-cycles the
board (korad) *before* flashing and again after, and GDS is started afterwards,
so GDS never outlives a board reset.

**Counter-evidence that keeps this unconfirmed:** the router counted only 3
bypassed packets, while the failing runs plus the manual send should have
produced more `CMD_NO_OP` attempts than that. So it is *not* established that
every attempt reached the board; some may have been lost on the uplink instead.
Do not treat the desync theory as settled.

### How to settle it

Cheapest first:

1. **Clean-slate bench run, mimicking CI ordering.** Power-cycle (or
   `reset run`) the board, *then* start GDS, *then* run
   `make test-integration FILTER=provision_key` — with no SWD session attached
   and no resets while GDS lives. If it passes, the desync theory holds and the
   bench procedure (not the code) was at fault.
2. **Count uplink arrivals directly.** Before/after a single `CMD_NO_OP`, read
   `ComCcsdsUart::provesRouter.m_routedPackets` / `m_bypassedPackets` /
   `m_rejectedPackets` over SWD. A `+1` per attempt proves the uplink is intact
   and moves the problem entirely to the downlink/GDS side; no change proves the
   uplink is dropping frames. **Resume the target before detaching** — a halted
   board drops its USB CDC, which silently no-ops everything (this bit us).
3. **If it is the downlink**, check whether GDS's space-packet sequence check
   should reset on a detected discontinuity, and whether the deframer's APID
   sequence state needs a resync path after a spacecraft reboot. Note there is
   already a `sync-sequence-number` make target and a "Sync Sequence Number"
   CI step for the *anti-replay* counter — a different counter, but the same
   class of ground/flight desync.
4. **Let CI settle it.** CI has never been green on this branch, so the
   dispatch-table fix may simply expose this step for the first time. Push and
   read the result before doing more bench work.

### Independent of the cause: the fixture coupling is worth fixing

`recover_from_safe_mode` is `autouse=True` and pulls in `start_gds` for every
test in the directory, so one uncooperative `CMD_NO_OP` takes out the entire
suite in setup — including the very test whose job is to bootstrap the board
into a state where commanding works. Making that fixture opt-in (or having it
tolerate an unavailable link) would decouple "the board is keyless" from "no
test can run", and would make failures report as failures rather than errors.

---

## Issue 2 — No over-the-air recovery from a mis-provisioned key

### Status: confirmed behaviour, needs a decision.

The key store cannot be re-keyed from the ground once it holds a wrong key:

- `PROVISION_KEY` is refused when the store is non-empty (`KeyProvisionFailed`
  with `NotEmpty`) — by design, so an attacker cannot overwrite the key.
- `REMOVE_KEY` refuses to remove the **last** key (`LastKey`) — by design, so
  the board cannot be locked out.
- `ADD_KEY` requires an already-authenticated link, which a wrong key cannot
  provide.

Together these mean a board provisioned with the wrong value is unreachable:
every recovery command needs either an empty store or a valid key, and neither
is obtainable. Hit during this session's bench work; recovery required a
physical SWD erase of `keystore_partition`:

```
openocd ... -c "init; halt; flash erase_address 0x10400000 0x40000; reset run; exit"
```

after which littlefs re-formatted the partition on the next boot and the board
came back keyless (`valid=0`, `seqnum=0`).

This is fine on the bench and fatal in flight.

### Options to weigh

- **Accept it**, and make provisioning a controlled ground procedure with a
  verification read-back before the board is buttoned up. Cheapest; leaves a
  single-point failure with no in-orbit remedy.
- **Allow `ADD_KEY` from the bypass allowlist** so a second key can be added
  unauthenticated, then the bad one removed. Restores recoverability but
  substantially weakens the security posture — an attacker could inject a key.
- **Add an authenticated `CLEAR_KEY_STORE`/`FORMAT_KEYS`**, usable only over an
  authenticated link. Does not help if the *only* key is wrong.
- **Two-slot bootstrap**: provision two keys at manufacture (`AuthKeyStore` has
  2 slots), so a rotation error still leaves one working key. Recoverable
  without weakening bypass, at the cost of a provisioning-procedure change.
- **Time-boxed bypass window after boot**, e.g. accept `PROVISION_KEY` on a
  non-empty store only within N seconds of reset. Recoverable via a power cycle;
  needs care that it is not a standing attack window.

No option is obviously right — this is a security/recoverability trade the
project owners should make explicitly, not a bug to be quietly patched.

---

## Bench procedure notes (learned the hard way)

- **Always resume the target before detaching from OpenOCD/GDB.** A halted board
  drops its USB CDC; GDS then sees nothing and commands silently no-op. Several
  hours of this session were spent chasing failures that were only a halted
  board.
- **Do not reset the board while GDS is running** — it desyncs the downlink
  deframer and is the leading suspect for Issue 1. Restart GDS after any reset.
- **`make build` does not re-derive Kconfig from device-tree changes.** Use
  `make generate build`; a stale `CONFIG_FLASH_SIZE` invalidated several bisect
  results in this session.
- Use `/dev/cu.*`, not `/dev/tty.*`, when reading the board CDC from macOS — a
  `tty.` open blocks on carrier detect and looks like a dead link.
- The `--active` flag on the vendored `uv` can resolve to a stale system
  `fprime_gds`; `fprime-venv/bin/fprime-cli` is the reliable path, and it needs
  `--deployment build-artifacts/zephyr/fprime-zephyr-deployment`.

Diagnostics live in `scripts/diag/` (see the ADR item in `TODO.md`).
