# OTA Downlink Performance — Measured Results

Measured 2026-06-25 on the bench rig (SF/BW sweep).

**File:** `//tp_asm.bin` (102 400 bytes, CRC `0xb6a19aa4`)
**Path:** flight LoRa TX → GRC SX1276 RX → USB → bridge GDS → `/tmp/ossf-1/fprime-downlink/`
**Pacing:** `downlinkDelay.DIVIDER_PRM_SET = 1` (fastest), `telemetryDelay.DIVIDER_PRM_SET = 60` (TM suppressed)
**Truth gate:** local `zlib.crc32 ^ 0xFFFFFFFF` of received file (no on-board verify needed for downlink)

---

## Throughput table

| Config      | Time (s) | Rate (B/s) | vs SF8 baseline | Notes               |
|-------------|----------|------------|-----------------|---------------------|
| SF8/BW125   | 356      | 287.6      | 1.00×           | baseline, no stalls |
| SF7/BW125   | 261      | 392.3      | 1.36×           | CRC-clean           |
| SF7/BW250   | 171      | 598.8      | 2.08×           | CRC-clean           |
| SF7/BW500   | 96       | 1066.6     | 3.71×           | CRC-clean           |

All runs delivered CRC-clean on first attempt.  No GRC stalls observed during any downlink.

---

## Comparison: downlink vs uplink

| Config    | Downlink (B/s) | Uplink (B/s) | DL/UL ratio |
|-----------|----------------|--------------|-------------|
| SF8/BW125 | 287.6          | ~258         | 1.12×       |
| SF7/BW125 | 392.3          | ~447         | 0.88×       |
| SF7/BW250 | 598.8          | ~779         | 0.77×       |
| SF7/BW500 | 1066.6         | ~1299        | 0.82×       |

Downlink runs ~15–25 % slower than uplink at the same SF/BW. Likely causes:
- Flight's `fileDownlink` credit-return chain (`loraRetry → downlinkDelay → framer`) adds one credit-return cycle per packet vs the GDS uplinker's explicit cooldown.
- Flight `fileDownlink` cycle time = 1 s (from `FileHandlingConfig.fpp`); paced at DIVIDER=1 = 100 ms between frames, but the credit chain still limits burst pacing.

---

## SF/BW switch procedure for downlink

Unlike uplink (where the GRC TX event automatically re-arms RX), downlink requires an explicit GRC modem re-arm because the GRC may not TX between config changes.

```bash
T=PROVESFlightControllerReference/test/int/bridge_uplink_test.py
FC_PYTEST=(./fprime-venv/bin/pytest -s -o addopts=""
  --dictionary build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json
  --zmq-transport ipc:///tmp/fc-server-in ipc:///tmp/fc-server-out --tts-port 50070)

# 1. Disable flight TX
CMD="ReferenceDeployment.lora.TRANSMIT" ARGS="DISABLED" REPS=1 "${FC_PYTEST[@]}" $T::test_send_cmd

# 2. Set flight params (SF_7 / BW_250_KHZ / BW_500_KHZ as needed)
CMD="ReferenceDeployment.lora.DATA_RATE_PRM_SET"   ARGS="SF_7"      "${FC_PYTEST[@]}" $T::test_send_cmd
CMD="ReferenceDeployment.lora.BANDWIDTH_TX_PRM_SET" ARGS="BW_250_KHZ" "${FC_PYTEST[@]}" $T::test_send_cmd
CMD="ReferenceDeployment.lora.BANDWIDTH_RX_PRM_SET" ARGS="BW_250_KHZ" "${FC_PYTEST[@]}" $T::test_send_cmd

# 3. Set GRC params + re-arm modem (SET_FREQ calls enableRx() with new params)
bash /tmp/grc_cmd.sh ReferenceDeployment.uhf.DATA_RATE_PRM_SET    SF_7
bash /tmp/grc_cmd.sh ReferenceDeployment.uhf.BANDWIDTH_TX_PRM_SET BW_250_KHZ
bash /tmp/grc_cmd.sh ReferenceDeployment.uhf.BANDWIDTH_RX_PRM_SET BW_250_KHZ
bash /tmp/grc_cmd.sh ReferenceDeployment.uhf.SET_FREQ 437400000

# 4. Enable flight TX (applies new config)
CMD="ReferenceDeployment.lora.TRANSMIT" ARGS="ENABLED" REPS=1 "${FC_PYTEST[@]}" $T::test_send_cmd

# 5. Suppress TM and set pacing via bridge GDS (tts 50050)
TM_DIVIDER=60 SPRAY_N=8 "${PYTEST[@]}" $T::test_suppress_tm
CMD="ReferenceDeployment.downlinkDelay.DIVIDER_PRM_SET" ARGS="1" REPS=4 "${PYTEST[@]}" $T::test_send_cmd

# 6. Trigger downlink (SPRAY_N=1 critical — spraying causes GRC TX, flight sees START during TX window)
DL_SRC="//tp_asm.bin" DL_DEST="tp_asm.bin" SPRAY_N=1 "${PYTEST[@]}" $T::test_send_file

# 7. Verify CRC locally (no on-board verify needed)
python3 -c "import zlib; d=open('/tmp/ossf-1/fprime-downlink/tp_asm.bin','rb').read(); print('crc=0x%08x' % (zlib.crc32(d)^0xffffffff))"
```

**FC direct GDS requirement:** The `FC_PYTEST` commands require the FC direct GDS running on `/dev/cu.usbmodem21101`.
This conflicts with `console_logger.py`. Procedure:
1. Kill console_logger (`kill <pid>` or `pkill -f console_logger.py`)
2. Launch FC direct GDS: `nohup ./fprime-venv/bin/fprime-gds --uart-device /dev/cu.usbmodem21101 --uart-skip-port-check --gui none --zmq-transport ipc:///tmp/fc-server-in ipc:///tmp/fc-server-out --tts-port 50070 --logs /tmp/sweep/gdslogs/flight-run --dictionary build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json > /tmp/sweep/gdslogs/flight.log 2>&1 &`
3. After config changes, kill FC GDS and restart console_logger.
4. Downlink CRC is verified locally — console_logger not needed for downlink.

---

## Known limitations

- **`SPRAY_N=1` for SendFile is mandatory.** SPRAY_N>1 causes the GRC to TX multiple times, and the flight sends the START packet during one of those TX windows. The GDS then sees all subsequent DATA packets as unexpected and discards them.
- **downlinkDelay cycle time:** `FileHandlingConfig.fpp` `cycleTime=1000ms` means even at DIVIDER=1 the flight paces at the 10 Hz rate group. No further speedup is possible without firmware changes.
- **Half-duplex:** uplink and downlink cannot run simultaneously without severe contention. Use the file already on the flight FS for downlink sweeps.
- **GRC stall on CRC error:** If the GRC SX1276 receives a LoRa packet with a CRC error during downlink, it may stall RX. Recovery: `bash /tmp/grc_cmd.sh ReferenceDeployment.uhf.SET_FREQ 437400000`. No stalls were observed in this sweep.
