# OTA uplink best practices

Operational checklist for ground-to-flight firmware/file delivery over the LoRa
link. Detailed mechanics are in the companion docs; this is the operator's quick
reference.

See also:
- [REFERENCE.md](REFERENCE.md) — rig topology, ports, auth, GDS launch
- [OTA-DROP-PATCHING.md](OTA-DROP-PATCHING.md) — drop recovery loop details
- [OTA-TIER1-PERFORMANCE.md](OTA-TIER1-PERFORMANCE.md) — throughput numbers, pass capacity

---

## Pre-pass checklist

```bash
# 1. Confirm exactly ONE GDS holder per port
lsof /dev/cu.usbmodem21203 | grep -c comm   # must print 1
lsof /dev/cu.usbmodem21101 | wc -l          # must be 1 (console logger)

# 2. Link alive — GET_SEQ_NUM must land
bash /tmp/sweep/linkcheck.sh   # or: SPRAY_N=12 pytest ...::test_spray_seq

# 3. Seq in sync — GDS seq file >= flight stored seq
#    Read flight stored seq from EmitSequenceNumber event, compare to:
cat Framing/src/sequence_number.bin

# 4. Flight TX disabled (standard uplink mode)
TX_STATE=DISABLED SPRAY_N=12 pytest ...::test_spray_disable_tx
```

If the link is dark at step 2, see the **Link-dark diagnostic** section below.

---

## SF/BW selection by elevation

| Elevation | Config | Throughput | Per-pass capacity (5 min) |
|---|---|---|---|
| All (default) | SF8 / BW125 | ~258 B/s | ~69 KB |
| >15° | SF7 / BW125 | ~447 B/s | ~121 KB |
| >30° | SF7 / BW250 | ~779 B/s | ~210 KB |
| >60° (overhead, bench-verified) | SF7 / BW500 | ~1299 B/s | ~351 KB |

Switch SF/BW before the pass, not mid-pass. Return to SF8/BW125 after.

**Apply procedure (both sides must match):**

```bash
# GRC side
bash /tmp/grc_cmd.sh ReferenceDeployment.uhf.DATA_RATE_PRM_SET SF_7
bash /tmp/grc_cmd.sh ReferenceDeployment.uhf.BANDWIDTH_TX_PRM_SET BW_250_KHZ

# Flight side — set params, then apply via TRANSMIT toggle
bash /tmp/fc_cmd.sh ReferenceDeployment.lora.DATA_RATE_PRM_SET SF_7
bash /tmp/fc_cmd.sh ReferenceDeployment.lora.BANDWIDTH_TX_PRM_SET BW_250_KHZ
bash /tmp/fc_cmd.sh ReferenceDeployment.lora.BANDWIDTH_RX_PRM_SET BW_250_KHZ
bash /tmp/fc_cmd.sh ReferenceDeployment.lora.TRANSMIT ENABLED   # must be from DISABLED state
bash /tmp/fc_cmd.sh ReferenceDeployment.lora.TRANSMIT DISABLED
bash /tmp/sweep/linkcheck.sh   # verify COUNT > 0 before uploading
```

**TRANSMIT ENABLED must be sent from DISABLED state.** After a normal
`ENABLED → DISABLED` cycle the state reaches DISABLED only after `dataIn_handler`
runs — wait for it (typically 1–2 seconds after the DISABLED command completes).
If in doubt, run `linkcheck.sh` first; if it passes, the state machine is ready.

---

## File delivery procedure

```bash
# 1. Copy source — uplinker deletes it on finish
cp firmware.bin /tmp/ota_src.bin

# 2. Compute truth CRC
python3 -c "import zlib; print('0x%08X'%(zlib.crc32(open('/tmp/ota_src.bin','rb').read())^0xffffffff))"

# 3. Uplink (TX disabled; timeout > actual transfer time)
UPLINK_SRC=/tmp/ota_src.bin UPLINK_DEST=//fw.bin UPLINK_TIMEOUT=400 \
  pytest ...::test_uplink_file

# 4. Verify on-board CRC
CRC_FILE=//fw.bin SPRAY_N=12 pytest ...::test_calc_crc
# Console: "CalculateCrcSucceeded : //fw.bin has CRC value 0x..."

# 5. If CRC mismatches — repeat steps 1–4 (re-uplink to the same dest)
#    Full re-uplink is idempotent: landed chunks are overwritten in place.
#    At ~0.6 %/frame loss, 1–2 rounds delivers any file intact.
```

See [OTA-DROP-PATCHING.md](OTA-DROP-PATCHING.md) for the automated `tier1-patch.sh`
driver which handles multi-part delivery, per-part CRC gating, and reassembly.

---

## Pass capacity planning (ISS orbit, SoCal ground station)

| Image | SF8/BW125 | SF7/BW125 | SF7/BW250 | SF7/BW500 |
|---|---|---|---|---|
| 100 KB | 2 passes (~1 day) | 1–2 passes | 1 pass | 1 pass |
| 350 KB | 5–7 passes (~2 days) | 3–4 passes (~1 day) | 2 passes | 1 pass |
| 700 KB | 10–13 passes (~2–3 days) | 6–8 passes (~1–2 days) | 3–4 passes (~1 day) | 2 passes |

Assumes ~4–6 passes/day, ~4–7 min usable/pass (ISS-orbit CubeSat, 408 km, 51.6°
incl., 34°N station). These include ~20% overhead for per-part CRC verify and
reassembly. BW500 is bench-validated; confirm link margin before using at operating
range.

---

## Post-pass: return to safe state

```bash
# Revert to SF8/BW125 (both sides)
bash /tmp/grc_cmd.sh ReferenceDeployment.uhf.DATA_RATE_PRM_SET SF_8
bash /tmp/grc_cmd.sh ReferenceDeployment.uhf.BANDWIDTH_TX_PRM_SET BW_125_KHZ
bash /tmp/fc_cmd.sh  ReferenceDeployment.lora.DATA_RATE_PRM_SET SF_8
bash /tmp/fc_cmd.sh  ReferenceDeployment.lora.BANDWIDTH_TX_PRM_SET BW_125_KHZ
bash /tmp/fc_cmd.sh  ReferenceDeployment.lora.BANDWIDTH_RX_PRM_SET BW_125_KHZ
bash /tmp/fc_cmd.sh  ReferenceDeployment.lora.TRANSMIT ENABLED
bash /tmp/fc_cmd.sh  ReferenceDeployment.lora.TRANSMIT DISABLED
```

Flight TX stays DISABLED between passes. SF/BW params are RAM-only — a power-cycle
reverts to the stored defaults (SF8/BW125). Write params to flash with
`DATA_RATE_PRM_SAVE` / `BANDWIDTH_TX_PRM_SAVE` / `BANDWIDTH_RX_PRM_SAVE` only when
intentional.

---

## DON'Ts

| Don't | Why |
|---|---|
| Use `lora.CONTINUOUS_WAVE` to apply SF/BW | Wedges the modem (issue #207 fix uncommitted in current binary) — leaves RX dead; requires power-cycle |
| Trust `FileReceived` as an integrity signal | Logs unconditionally on the END packet, even if frames were dropped |
| Trust `dropDetector.DETECT_DROPS` | Non-deterministic false positives on this build; use `CalculateCrc` |
| Spray `AppendFile` | Not idempotent — each send appends a copy; corrupts reassembly |
| Use chunk size > 200 B | Frame overhead pushes authenticated TC frame over 248 B → `FrameTooLarge` silent drop |
| Set `UPLINK_TIMEOUT` short | Aborts the transfer mid-stream; file is left holed |
| Send `TRANSMIT ENABLED` from DISABLING state | No `comStatusOut` fires → modem not reconfigured; power-cycle required |

---

## Link-dark diagnostic

If `linkcheck.sh` returns COUNT=0:

```
1. Check GDS holders:  lsof /dev/cu.usbmodem21203  → must be exactly 1
   If >1: pkill -f fprime_gds.executables.comm; pkill -f CustomDataHandlers

2. Flight alive?  bash /tmp/fc_cmd.sh CdhCore.cmdDisp.CMD_NO_OP
   → "NoOpReceived" on 21101 console = flight alive, LoRa is the problem

3. LoRa modem state:
   - TRANSMIT ENABLED from DISABLED state → then TRANSMIT DISABLED → re-check
   - If already sent CONTINUOUS_WAVE: power-cycle required (modem wedged,
     no software recovery once the ComQueue credit is spent)

4. Seq out of window:  read EmitSequenceNumber from console; advance seq file
   past it and restart GDS
```
