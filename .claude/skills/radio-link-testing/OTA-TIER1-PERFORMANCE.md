# Tier-1 part-level OTA patch — performance & pass-window optimization

Tier 1 = split the image into independent part-files, upload each, verify each with
on-board `CalculateCrc`, re-upload only the parts whose CRC is wrong, reassemble with
`AppendFile`, verify the whole. It uses only working primitives (no sparse uplinker, no
DropDetector). Driver: `scripts/tier1-patch.sh <source> <num-parts> [max-rounds]`.
Method + verification details: [OTA-DROP-PATCHING.md](OTA-DROP-PATCHING.md).

**Validated end-to-end on the bench (2026-06-20):** 24 KB → 6×4 KB parts, each uploaded
(16–17 s) and CRC-verified, reassembled, final `//tp_asm.bin` CRC `0x3dd5ffb9` == truth.

## Measured constants (BW125 / CR4-5, cooldown 0.4 s)

| Quantity | SF8 (baseline) | SF7 (measured 2026-06-20) |
|---|---|---|
| Effective uplink goodput | **256 B/s** (4096 B/16 s, ×6) | **476–477 B/s** (20480 B/42.9–43.0 s, ×2) = **1.86×** |
| Per 204 B chunk | **0.8 s** (1.25 chunks/s) | **0.42–0.43 s** (2.35 chunks/s) |
| LoRa frame airtime floor | ~0.69 s/frame (SF8) | ~0.40 s/frame (SF7) |
| Per-chunk over-air loss | ~0.6 % (3/502) | ~1–2 % observed (1 of 2 runs: 2 drops/101; other run clean) |
| Usable pass window | ~270 s (4.5 min) | ~270 s |
| Per-pass clean upload | **~67 KB / ~337 chunks** | **~128 KB / ~630 chunks** |

SF7 measured exactly as predicted (~0.43 s/chunk, ~2×). A dropped run was recovered to a
clean whole-file CRC by one idempotent re-uplink (drop-patch loop) — see [[ota-drop-patch-loop]].

**Cooldown is already below the airtime floor**, so throughput is PHY-bound — lowering
cooldown won't help (it only risks GRC overflow). The lever that moves throughput is the
**spreading factor** — now verified on the bench, not just modeled.

## Where the time goes

The initial upload (every byte sent once) dominates; patch re-sends are ~10–25 %, verify and
reassembly less. So an update can't fit in one pass (100 KB ≈ 6.7 min, 666 KB ≈ 45 min at
SF8) — **updates span multiple passes, and the goal is the fewest passes with zero wasted
re-work.**

| Image | SF8 (~0.8 s/chunk) | SF7 (~0.43 s/chunk, ≈2×) |
|---|---|---|
| 100 KB (502 chunks) | ~6.7 min → **2 passes** | ~3.6 min → **1 pass** |
| 666 KB (3343 chunks) | ~45 min → **~12 passes** | ~24 min → **~7 passes** |

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

1. **Spreading factor — biggest lever.** SF8→SF7 ≈ halves airtime → ≈2× throughput → ≈half
   the passes, for ~2.5 dB of margin. Use **SF7 near closest approach** (high elevation),
   fall back SF8/SF9 at low elevation. Even if SF7 doubles loss to ~1.2 %, the extra re-send
   (~+12 % at k=20) is dwarfed by the 2× speedup. `lora.DATA_RATE_PRM_SET SF_7` exists.
2. **Leave cooldown at 0.4 s** (already airtime-bound); raise only if loss spikes.
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

~4–8 KB parts, SF7 near closest approach (SF8 fallback), `CalculateCrc` verification
(manifest-batched for firmware), resume by verified-part set: **100 KB in 1–2 passes, a
666 KB firmware in ~6–12 passes** depending on SF — initial upload is the irreducible floor,
patch overhead held to ~10–25 %, and no pass's work is ever lost to LOS.
