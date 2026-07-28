# Investigation: integration tests green on the local bench and in CI

**Goal: `provision_key_test.py` and the rest of the integration suite pass both
on the local bench and in CI, on a board that starts keyless.**

Status as of 2026-07-28: **both are green.**

- **Bench: 30 passed, 0 failed**, on a flight control board with a face
  attached, no battery board, no antenna board and JP6 open.
- **CI: run 30321256393 passes all six jobs** — `lint`, `unit-test`, `build`,
  `yamcs-build`, `integration-uart`, `integration-radio`. First green CI on
  this branch.

Both key-store paths are verified on hardware: a freshly erased (keyless) board
provisions and then authenticates, and an already-provisioned board reports
`NotEmpty` and is treated as success. CI exercises the keyless path for real —
its board had never been provisioned, since no prior run on this branch got
past boot.

The last CI-only defect was `Start YAMCS Stack` missing `PROVES_AUTH_KEY`: it
runs `make yamcs` → `tools/yamcs/proves_adapter.py`, which resolves the HMAC key
at construction. Free while the key was compiled into the image; with the key on
the satellite the adapter died at startup, YAMCS had no uplink, and
`test_noop_round_trip` timed out. Fixed in `622d8661`.

One item remains open, and it is a design decision rather than a defect:
recovery from a board provisioned with the wrong key (§2).

---

## Issue 1 — `provision_key_test.py` never reached its test body — RESOLVED

The previous edition of this file suspected a GDS downlink desync caused by
resetting the board underneath a long-lived GDS. **That theory was wrong.** Two
real defects were behind it, plus one bench-procedure artifact.

### Cause A (the actual test failure): a mis-built event predicate

`provision_key_test.py` awaited "either KeyProvisioned or KeyProvisionFailed"
with

```python
await_event(satisfies_any([get_event_pred(...), get_event_pred(...)]))
```

`IntegrationTestAPI.get_event_pred` returns its argument unchanged only when it
is already an `event_predicate`. A `satisfies_any` is a predicate but *not* an
`event_predicate`, so it fell through to being used as the **event-ID**
predicate — the two inner `EventData` checks were evaluated against an integer
id and could never be true. The search timed out even though the event had
arrived, and `evt` came back `None`, which then blew up as
`AttributeError: 'NoneType' object has no attribute 'template'`.

Fixed by matching over ids instead:

```python
await_event(is_a_member_of([translate_event_name(...), translate_event_name(...)]))
```

plus an explicit `assert evt is not None` so a genuine no-response failure
reports as itself rather than as an `AttributeError`.

### Cause B (why it looked like a setup error): a halted board

The earlier bench sessions had left the target halted under SWD. A halted board
drops its USB CDC, so GDS saw nothing and `start_gds`'s `CMD_NO_OP` loop failed
— erroring every test in the directory during setup. With the board simply left
running, `start_gds` passes first time.

`start_gds` used a bare `assert gds_working`, which is why this presented as an
unexplained error on 40-odd tests. It now reports the command, the attempt
count and the last exception.

### Not a cause: the space-packet sequence-count warning

GDS does log

```
[WARNING] framing: APID 2 received sequence count: 35 (expected: 1)
```

on every startup against an already-running board, but it is a warning only —
GDS adopts the received count and carries on. `reset_manager`'s cold- and
warm-reset tests both pass with GDS running across the reboot.

---

## Bench-only failures, and what they actually were

Four tests failed on the bench for reasons unrelated to the key store. Each is
now either fixed or correctly marked.

| Test | Cause | Resolution |
| --- | --- | --- |
| `tmp112`, `veml6031` | With nothing at the battery terminals the power monitor reads 0.012 V, so modeManager auto-enters SAFE_MODE (`reason=LOW_BATTERY`) every debounce period; its safe-mode sequence switches the face load switch **off**, and enough of those cycles wedges the face I2C bus for the rest of the session. | New `--no-battery` pytest option drops `SafeModeEntryVoltage` to 0 before each test, so auto-entry never fires. Verified: 0 `AutoSafeModeEntry` events in a full run. |
| `antenna_deployer::test_deployment_prevention_after_success` | After `format_filesystem`, `/antenna` no longer exists, so `SET_DEPLOYMENT_STATE` fails with `FileOperationError ... on file open_write`. The directory is recreated at boot. | Bench procedure: power-cycle after formatting, which is what CI already does (`Format Filesystem` → `Power-Cycle Satellite`). Also added an `exit_safe_mode` to its fixture, since deployment is inhibited in safe mode. |
| `rtc_test::test_04_sequence_cancellation_on_time_set` | `uplink_sequence_and_await_completion` fired `CreateDirectory /seq` and uplinked immediately, racing the mkdir: `FileOpenError: Could not open file /seq/no_op.bin` 30 ms *before* `CreateDirectorySucceeded`. Only visible when `/seq` did not already exist. | Real pre-existing race, fixed: wait for `CreateDirectorySucceeded` **or** `DirectoryCreateError` (already-exists) before uplinking. |
| `drv2605::test_01_magnetorquer_power_draw` | Asserts a ≥0.3 W rise in INA219 system power; that rail reads 0.0 W in both samples without a battery. No command can fix an unpowered rail. | Marked `requires_battery` in addition to `requires_face`. |
| `mode_manager::test_safe_09` | Asserts the boot count increments after a watchdog-driven hardware power cycle; with JP6 open the reboot never happens (177 → 177). The command-loss detection and SAFE_MODE entry it also asserts both pass. | Marked `requires_watchdog_jumper`. |

These markers are inert in CI: CI never passes `--bare-flight-controler-board`,
which is the only thing that acts on them.

### Reproducing the green bench run

```sh
# 1. board running and NOT halted under SWD; no GDS yet
PROVES_AUTH_KEY=<32 hex chars> make gds-integration UART_DEVICE=/dev/cu.usbmodem1101 &

# 2. bootstrap, in CI's order
make test-integration FILTER=provision_key
make test-integration FILTER=sync_sequence_number

# 3. the suite
make test-integration \
  FILTER="not sync_sequence_number and not format_filesystem and not provision_key \
          and not requires_antenna and not requires_battery and not requires_watchdog_jumper" \
  PYTEST_ARGS=--no-battery
```

If you run `format_filesystem`, reset the board before the suite so `/antenna`
and friends are recreated.

---

## Issue 2 — No over-the-air recovery from a mis-provisioned key

### Status: confirmed behaviour, still needs a decision.

The key store cannot be re-keyed from the ground once it holds a wrong key:

- `PROVISION_KEY` is refused when the store is non-empty (`KeyProvisionFailed`
  with `NotEmpty`) — by design, so an attacker cannot overwrite the key.
- `REMOVE_KEY` refuses to remove the **last** key (`LastKey`) — by design, so
  the board cannot be locked out.
- `ADD_KEY` requires an already-authenticated link, which a wrong key cannot
  provide.

Together these mean a board provisioned with the wrong value is unreachable:
every recovery command needs either an empty store or a valid key, and neither
is obtainable. Recovery on the bench requires a physical SWD erase:

```
openocd ... -c "init; halt; flash erase_address 0x10400000 0x40000; reset run; exit"
```

after which littlefs re-formats the partition on the next boot and the board
comes back keyless (`valid=0`, `seqnum=0`). This was used repeatedly during
this session's verification and works reliably.

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
  drops its USB CDC; GDS then sees nothing and commands silently no-op. This was
  the single biggest time sink across two sessions, and it is what made Issue 1
  look like a product defect.
- Resetting the board while GDS runs is **fine** — GDS logs a sequence-count
  warning and resyncs. (The earlier claim to the contrary was wrong.)
- After `format_filesystem`, **reset the board** before running the suite:
  `/antenna` and `/seq` are recreated at boot.
- **`make build` does not re-derive Kconfig from device-tree changes.** Use
  `make generate build`; a stale `CONFIG_FLASH_SIZE` invalidated several bisect
  results in an earlier session.
- Use `/dev/cu.*`, not `/dev/tty.*`, when reading the board CDC from macOS — a
  `tty.` open blocks on carrier detect and looks like a dead link.
- OpenOCD lives at `~/code/github.com/raspberrypi/openocd` (the raspberrypi
  fork — do not substitute a nix/brew build).
- The `--active` flag on the vendored `uv` can resolve to a stale system
  `fprime_gds`; `fprime-venv/bin/fprime-cli` is the reliable path, and it needs
  `--deployment build-artifacts/zephyr/fprime-zephyr-deployment`.

Diagnostics live in `scripts/diag/` (see the ADR item in `TODO.md`). The
`Hang Forensics` CI steps that drove them have been removed — they halted the
target over SWD immediately before GDS started, which is the exact failure mode
above.
