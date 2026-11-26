# UPZ: compressed file uplink (prototype)

Compress a file on the ground, uplink the smaller `.upz`, and let
`ReferenceDeployment.fileExpander` rebuild the exact original on board during the
idle time between passes.

On the hop-1 Electra image (716,808 B) a UPZ file is 242,612 B, a ratio of 0.338:
26 uplink segments of 9,600 B instead of 75.

## How it works

1. A Thumb-2 branch filter rewrites the PC-relative offset of every `BL` and
   `B.W` to an absolute target, per 256 B block, so repeated calls to one
   function become identical bytes. It is worth about 17%.
2. Raw LZMA (`lc=0 lp=1 pb=1`, 1 MiB window) compresses the filtered bytes in
   64 KiB chunks. Each chunk starts from a fresh LZMA state but may match back
   into every earlier byte, which costs about 1.4% and lets the board resume at
   any chunk without saved coder state.
3. On board, the decoder keeps 4 KiB of recent output in RAM and reads older
   window bytes back from the output file. Its state is 11.6 KB; no PSRAM is
   needed. It runs in bounded steps from the 1 Hz rate group: at most 4 KiB
   decoded and 64 storage calls per tick.

Progress lives in the output file. After a reset, sending the same
`EXPAND_FILE` again keeps every leading chunk whose CRC matches the table and
decodes from the first one that does not. The finished output is re-read from
storage and checked against the whole-file CRC before `ExpandComplete` fires.
If storage disagrees, the decoder rescans once and redoes the bad chunks.

The container layout is documented in
`PROVESFlightControllerReference/Components/FileExpander/UpzExpander.hpp`.

## Use

```bash
python3 tools/upz/upz.py compress zephyr.signed.bin image.upz   # checks the round trip before writing
python3 tools/upz/upz.py info image.upz
```

Uplink `image.upz` as usual, check its CRC, then:

```text
ReferenceDeployment.fileExpander.EXPAND_FILE /image.upz /image.bin
# wait for ExpandComplete(out_file, raw_size, crc32, far_reads, ticks)
FileHandling.fileManager.CalculateCrc /image.bin      # reports the CRC inverted
Update.updater.UPDATE_IMAGE_FROM /image.bin           # images only
```

Use `--no-filter` for files that are not Thumb code. Needs liblzma (`xz-utils`).

## Tests

- `python3 tools/upz/test_upz.py` builds the flight decoder on the host (with
  ASan/UBSan when available) and checks round trips, random resets, torn and
  stale output, storage upsets and corrupt streams. `make test-unit` runs it.
- `PROVESFlightControllerReference/test/int/file_expander_test.py` uplinks
  150 KB of the build's firmware in UPZ form, expands it on the board, checks
  the CRC, then aborts an expansion midway and checks that it resumes.
