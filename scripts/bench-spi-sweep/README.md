# bench-spi-sweep

Standalone Zephyr app that measures SX1262 `ReadBuffer` (252 B) cost on the v5e
over the PL022 SPI1 port, across SCK clocks and PL022 driver modes (polled,
interrupt, DMA). Written for proves-core-reference#602. Results are in
`results/`.

## What it does

1. Resets the radio (`reset-gpios`), waits for BUSY low, sends `SetStandby(STDBY_RC)`,
   reads `GetStatus` and registers `0x0740/0x0741` (LoRa sync word, reset value
   `0x14 0x24`) to confirm an SX126x is on the bus.
2. For each requested clock in `{125 k, 1 M, 4 M, 8 M, 12 M, 16 M, 18.75 M}` Hz,
   1000 times: `WriteBuffer` a 252 B xorshift pattern, wait BUSY, `ReadBuffer` it
   back (255 B frame), compare.
   Pass `same` writes and reads at the sweep clock. Pass `wr1m` writes at 1 MHz
   and reads at the sweep clock, to isolate read-path (MISO) corruption.
3. Times only the `ReadBuffer` `spi_transceive()` with the DWT cycle counter.
   A lowest-priority thread counts while it owns the CPU; the drop in its count
   during the call converts to CPU time the call left free, so `cpu_*` columns are
   the CPU actually consumed (polled: equals wall time; IRQ/DMA: ISR and context
   switch cost only).
4. Reads back `SSPCR0.SCR` and `SSPCPSR` after the first transfer and reports the
   SCK the PL022 actually produced from `clk_peri` (150 MHz).
5. Prints `CSV,...` rows on the USB CDC console.

The 18.75 MHz point is above the SX1262's 16 MHz limit and is a signal-margin
probe only.

## Rerun

Build on a machine with this repo's west workspace and `keys/proves.pem`:

```sh
scripts/bench-spi-sweep/build.sh /path/to/proves-core-reference /tmp/b602
```

That produces `/tmp/b602/{poll,irq,dma}/zephyr/zephyr.signed.bin`, linked into
slot0 and signed for the project's MCUboot, so the bootloader on the board is
untouched. Copy them to the Pi as `img/{poll,irq,dma}.signed.bin` together with
`run-on-bench.sh` and `backup-restore.sh`, then on the dev bench:

```sh
./backup-restore.sh backup before.bin        # bootloader + slot0 + slot1
./run-on-bench.sh img out                     # flashes each mode, captures console-<mode>.log
./backup-restore.sh restore before.bin        # put the flight image back
```

`run-on-bench.sh` refuses to run if anything holds `/dev/ttyDEVBOARD` or the dev
probe. `table.py out/console-*.log` renders the CSV rows as a Markdown table.
