# Tier-1 part-level OTA patch — performance & pass-window optimization

Tier 1 = split the image into independent part-files, upload each, verify each with
on-board `CalculateCrc`, re-upload only the parts whose CRC is wrong, reassemble with
`AppendFile`, verify the whole. It uses only working primitives (no sparse uplinker, no
DropDetector). Driver: `scripts/tier1-patch.sh <source> <num-parts> [max-rounds]`.
Method + verification details: [OTA-DROP-PATCHING.md](OTA-DROP-PATCHING.md).

**Validated end-to-end on the bench (2026-06-20):** 24 KB → 6×4 KB parts, each uploaded
(16–17 s) and CRC-verified, reassembled, final `//tp_asm.bin` CRC `0x3dd5ffb9` == truth.

## Measured constants (BW125 / CR4-5, frame-aware bridge, cd=0.0, chunk=200 B)

All numbers below are with the **frame-aware GRC ByteComBridge** (`c1a941f`, 2026-06-24),
CRC-verified (fileManager.CalculateCrc == truth).

| Config | Goodput | vs SF8 | Per-chunk | Pass capacity |
|---|---|---|---|---|
| **SF8/BW125** (baseline) | **~258 B/s** | 1× | ~0.78 s | ~69 KB / ~345 chunks |
| **SF7/BW125** | **~447 B/s** | **1.73×** | ~0.45 s | ~121 KB / ~602 chunks |
| **SF7/BW250** | **~779 B/s** | **3.02×** | ~0.26 s | ~210 KB / ~1049 chunks |
| **SF7/BW500** | **~1299 B/s** | **5.03×** | ~0.15 s | ~351 KB / ~1755 chunks |

All measured 2026-06-25 with 4 KB file, chunk=200 B, cd=0.0. CRC PASS ✓ at all configs.

**Old bridge numbers (historical, OLD circular-buffer bridge):** SF8 ~228 B/s (cd=0.9), SF7
~336 B/s (cd=0.6), bottleneck was GRC drain cadence (~0.6 s/frame), not RF. The frame-aware
bridge eliminates that bottleneck — throughput is now purely airtime-bound.

**BW250/BW500 require matching on both sides.** Set `BANDWIDTH_TX_PRM_SET` (GRC) and both
`BANDWIDTH_TX_PRM_SET` + `BANDWIDTH_RX_PRM_SET` (flight). Apply via `TRANSMIT ENABLED →
DISABLED` from DISABLED state (see [[sf7-switch-apply-mechanics]]).

SF7 (2026-06-20, old bridge, different procedure) measured ~476–477 B/s. The difference is
measurement method; both are consistent with the airtime model.

**Cooldown is already below the airtime floor**, so throughput is PHY-bound — lowering
cooldown won't help (it only risks GRC overflow). The lever that moves throughput is the
**spreading factor** — now verified on the bench, not just modeled.

## Where the time goes

The initial upload (every byte sent once) dominates; patch re-sends are ~10–25 %, verify and
reassembly less. So an update can't fit in one pass (100 KB ≈ 6.7 min, 666 KB ≈ 45 min at
SF8) — **updates span multiple passes, and the goal is the fewest passes with zero wasted
re-work.**

| Image | SF8/BW125 (~0.78 s/chunk) | SF7/BW125 (~0.45 s/chunk) | SF7/BW250 (~0.26 s/chunk) | SF7/BW500 (~0.15 s/chunk) |
|---|---|---|---|---|
| 100 KB (512 chunks) | ~6.7 min → **2 passes** | ~3.8 min → **1 pass** | ~2.2 min → **1 pass** | ~1.3 min → **1 pass** |
| 666 KB (3410 chunks) | ~44 min → **~10 passes** | ~26 min → **~6 passes** | ~15 min → **~4 passes** | ~8.5 min → **~2 passes** |

(includes ~20 % patch+overhead, rounded up to whole passes)

## Cost model

Total ≈ `C·t_c·(1 + k·p)` + `(C/k)·v` where C = total chunks, k = chunks/part,
p = per-chunk loss, t_c = 0.8 s, v = per-part overhead (CRC verify + reassembly append).

- `C·t_c` — initial upload (irreducible floor).
- `C·t_c·k·p` — re-upload waste: a dropped frame forces re-sending its whole **k-chunk part**,
  so waste grows with part size.
- `(C/k)·v` — verify + append overhead: grows with part **count**.

Minimizing over k: **k\* = √(v /(t_c·p)) ≈ √(2.5/0.006) ≈ 20 chunks (~4 KB)**. The reassembly
cost (one `AppendFile` per part) nudges the practical optimum up a little → **4–8 KB parts**.

## Optimization levers, ranked

1. **Spreading factor + bandwidth — by far the biggest levers.** With the frame-aware bridge,
   throughput is purely airtime-bound. SF8→SF7 = 1.73×; SF7+BW250 = 3.02×; SF7+BW500 = 5.03×
   (all bench-verified, CRC-clean). Use the fastest config the link supports at the current
   elevation. Apply via `DATA_RATE_PRM_SET` + `BANDWIDTH_*_PRM_SET` then `TRANSMIT ENABLED →
   DISABLED` (from DISABLED state). BW500 requires high link margin (bench-only until
   link-budget analysis at operating range; BW250 is a safer intermediate step).
2. **Cooldown is already eliminated** (frame-aware bridge, cd=0.0 CRC-clean); no further tuning.
3. **Part size ≈ 20–40 chunks (4–8 KB).** Never pass-sized — one dropped frame then wastes a
   whole pass. Small parts also give ~15 checkpoints per pass.
4. **Verify with `CalculateCrc`; batch into a CRC manifest for big images.** Per pass you only
   verify the ~15 parts you just sent (~30 s, ~10 % of the pass). For the 666 KB image's
   ~150 parts, a single packed-CRC-manifest downlink beats 150 individual result frames —
   worth a small FSW addition.

## Resumability (what makes pass windows work)

Each part is an independent file, so the update is checkpointed at part granularity:
- Ground keeps a **verified-good part set**; never re-upload a verified part.
- Per pass: uplink burst (flight **TX disabled** — reliable RX) of the next not-good parts
  until LOS, then a short **TX-enabled** burst to downlink the CRCs. A part interrupted by LOS
  is just re-sent next pass — no corrupt partial state, because a part is marked good only
  after its CRC matches (so ≤ one part, ~16 s, is ever wasted at a pass boundary).
- Reassemble + whole-file CRC + hand to `Update.updater.UPDATE_IMAGE_FROM` only after **all**
  parts verify — can be its own short final pass.

## Gotchas the bench surfaced

- **`uplink_file_and_await_completion` never returns in TX-disabled mode** (completion needs
  downlink) — background the kick and judge by `FileReceived` on the console.
- **`AppendFile` is NOT idempotent** — spraying it appends the part multiple times and
  corrupts the reassembly. Send it **once** per part and gate on `AppendFileSucceeded`
  (idempotent commands like `CalculateCrc`/`RemoveFile` are fine to spray). `RemoveFile` the
  reassembly target before the first append so a retried run doesn't concatenate onto stale
  data.
- Compare CRCs **numerically** — the board prints `0x{x}` (no leading zeros); a string compare
  against a zero-padded value gives false mismatches.
- Reassembly append-count is a real cost for big files (~N appends); an on-board "concat these
  parts" command would remove it (future work, overlaps with the Tier-2 sparse path).

## Bottom line

~4–8 KB parts, highest-SF+BW the link supports (SF7/BW500 on bench = 5× SF8; SF7/BW125 safe
at low elevations), `CalculateCrc` verification (manifest-batched for firmware), resume by
verified-part set: **100 KB in 1 pass at SF7/BW250+, a 666 KB firmware in ~2–6 passes
depending on SF/BW** — initial upload is the irreducible floor, patch overhead held to
~10–25 %, and no pass's work is ever lost to LOS.
