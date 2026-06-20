# OTA drop-patching: reliable file delivery over the lossy LoRa uplink

The LoRa uplink in TX-disabled mode has **no ARQ**, so on large files an occasional
frame is lost over-air → the on-board file has a hole → the CRC-gated OTA apply
(`Update.updater.UPDATE_IMAGE_FROM`) would reject it. This is the application-layer
recovery loop that turns "file arrived with a hole" into "file arrived intact",
verified on the bench rig 2026-06-20.

Prereqs: drive the rig with the **`radio-link-testing`** skill (launch GDS, sync seq,
disable flight TX, uplink). Run everything from the flight repo root with the venv pytest
and the explicit `--dictionary`.

## TL;DR method (deployable today)

```
loop:
  1. uplink  <truth> -> //dest.bin           # full file, every round
  2. verify  CalculateCrc //dest.bin == F'(truth)
  3. if mismatch -> goto 1 (patch round); else DONE (file is byte-for-byte intact)
```

Each round is a **full re-uplink to the same destination**. It is idempotent: chunks that
already landed are overwritten in place with identical data; only the still-missing chunks
matter. A chunk is wrong only if it was dropped in *every* round. At the measured ~0.6%
per-chunk loss, a 100 KB / 502-chunk file reaches intact in ~1–2 rounds.

## Why re-uplink doesn't clobber the good chunks (the key property)

`FileUplink` pre-declares the file size on the START packet and writes each DATA packet at
its absolute `byteOffset`:

- `FileUplink::File::open` opens with `Os::File::OPEN_WRITE`, which on the flight's Zephyr
  backend = `FS_O_CREATE | FS_O_WRITE` — **no `FS_O_TRUNC`**. Re-opening the same dest does
  not zero it. (`lib/fprime-zephyr/fprime-zephyr/Os/File.cpp`)
- Each DATA write does `seek(byteOffset, ABSOLUTE)` then `write(...)`, so chunks land at
  their true offsets regardless of arrival order, and a re-send overwrites in place.
  (`lib/fprime/Svc/FileUplink/File.cpp`)
- A dropped chunk is simply never written, so it reads back as zeros (the file is created
  sparse/zero-filled up to the highest received offset). NB: chunks dropped at the very
  *tail* (no later chunk) leave the file short rather than zero-filled — a full re-uplink
  fixes this because it always re-sends the final chunk.

## Verifying without downlinking — use CalculateCrc, NOT DropDetector

Verdict source = the flight USB console (`/tmp/flight-console-text.log`); grep with `LC_ALL=C`.

**Reliable oracle — `fileManager.CalculateCrc`:**
```
CRC_FILE=//dest.bin SPRAY_N=12 "${PYTEST[@]}" ...::test_calc_crc
# console: "CalculateCrcSucceeded : //dest.bin has CRC value 0x251825dd"
```
F´ `CalculateCrc` == **`zlib.crc32(data) ^ 0xFFFFFFFF`** (libcrc CRC-32 without zlib's final
complement). Verified stable (36/36 identical reads) and correct on 3 files. Compute the
expected value locally:
```
python3 -c "import zlib;print('0x%08X'%(zlib.crc32(open('truth.bin','rb').read())^0xffffffff))"
```
If the on-board CRC equals this, the file is byte-for-byte intact.

**Do NOT trust `dropDetector.DETECT_DROPS`.** On this flight build it returns
**non-deterministic false positives** — three scans of one *proven-intact* file reported
{99,100,195,468,469}, then {252,253}, then {119,120}. Its read path intermittently returns
all-zero buffers for non-zero data. Treat any DETECT_DROPS output as untrustworthy until the
detector is fixed (`lib/fprime-extras/.../DropDetector/DropDetector.cpp::readPacket`).

## Worked demonstration (bench, 2026-06-20)

- File: 100 KB, 502 chunks @ 204 B, all bytes non-zero. GDS `--file-uplink-cooldown 0.4`,
  flight TX DISABLED, seq synced.
- **Round 1** uplink → `FileReceived` + `BadChecksum` (computed 0xd9856706 ≠ read 0x6b3ee595):
  a real over-air drop, file corrupt.
- **Round 2** = re-uplink the same file to the same `//ota_q1.bin` → `FileReceived`, **no
  BadChecksum**. On-board `CalculateCrc //ota_q1.bin` = `0x251825dd` = F´(truth) (truth
  zlib 0xDAE7DA22 ^ 0xFFFFFFFF). **Intact.**

## Operational gotchas (learned the hard way)

- **`UPLINK_TIMEOUT` must exceed the real transfer time.** A 100 KB file takes ~5–7 min at
  0.4 s cooldown; a short timeout (e.g. 15 s) aborts the transfer mid-stream and the file is
  left badly holed. Use a generous value (≥400 s for 100 KB).
- The uplinker **deletes its source file** on finish → always uplink a throwaway `cp`.
- pytest's `uplink_file_and_await_completion` returns/“passes” well before the file is
  actually on the board (it tracks the local GDS→GRC handshake, not over-air delivery). Judge
  completion by `FileReceived //dest.bin` on the flight console, never by the pytest result.
- Re-sync the GDS seq file on every GDS restart — see the `radio-seq-number-mechanics` memory.
- Multiple console-logger processes on one USB port collide ("multiple access") and starve
  each other; keep exactly one per port.

## Composing into an OTA update

The apply half already exists (worktree branch `bridge-cse_019QXe…`,
`scripts/ota_update.py` + `Update.updater`): upload → `PREPARE_UPDATE` →
`UPDATE_IMAGE_FROM <file> <crc>` (CRC-gated) → `CONFIGURE_NEXT_BOOT TEST` → power-cycle →
`CONFIRM_UPDATE`. This drop-patch loop slots in **between upload and PREPARE**: re-uplink +
CalculateCrc-verify until the image CRC matches, so the CRC-gated apply never aborts on a
hole.

## Known limitation & future work (bandwidth efficiency)

Every patch round re-sends the **whole** file (~7 min for 100 KB; ~45 min for a 666 KB
image) even to fix a few chunks. Bandwidth-optimal recovery (re-send only the missing chunks
at their offsets) needs two things this stack doesn't have yet:

1. **A sparse/offset uplinker.** The stock GDS uplinker reads sequentially from offset 0
   (`fprime_gds/common/files/uplinker.py`) — it cannot emit only selected chunks. Options:
   a small custom packet emitter (START with full size + DATA at the missing offsets + END),
   or strategy **B**: `tools/bin/split-n-seq` to split the file, re-uplink only the dropped
   parts, and reassemble on-board with `fileManager.AppendFile` (sequence tooling already
   generated by `split-n-seq`).
2. **A reliable drop locator** to know which chunks to re-send — blocked on the DropDetector
   bug above. Until it's fixed, the cheap-to-locate, reliable approach is full re-uplink +
   CalculateCrc.
