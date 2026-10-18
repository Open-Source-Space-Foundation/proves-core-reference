# Bench drills

Scripts for the ready-to-merge drill protocol in
[#593](https://github.com/Open-Source-Space-Foundation/proves-core-reference/issues/593)
(the last comment is the spec). Each drill applies the universal gate and
prints an evidence block for the results comment.

| Entry | Drills |
|---|---|
| `d1.py` | D1 profile matrix, D1a stray-packet confirm, D1b revert cap, D1c unguarded, D1d CW bounds, D1s plain-config smoke |
| `d2.py` | D2 128 s regression |
| `d3.py` | D3 P3 saturation, D3b goodput sweep |
| `d4.py` | D4 uspRadio idle CPU |
| `d5.py` | D5 soak (6 h) |
| `d6.py` | D6 legacy-GRC P0 interop |
| `run_all.py` | The suite: every drill except D1s and D6 |

`gate.py` holds the universal gate and the evidence block. `rig.py` holds the
bench I/O. `ops.py` holds the radio steps ported from July's
`logs/resume-0710` scripts.

## Build the flight image with the overlay

`observability.conf` adds an RTT console, thread names, runtime stats and an
RTT shell. The Makefile has no hook for an extra Kconfig file, so pass it to
CMake with `EXTRA_CONF_FILE` when you generate:

```sh
make generate
. fprime-venv/bin/activate
fprime-util generate --force \
  -DEXTRA_CONF_FILE="$PWD/scripts/bench-drills/observability.conf"
make build BUILD_YAMCS_MDB=0
```

Activate the venv first. Without it, CMake can pick another Python and the
devicetree step fails.

`make build` keeps the overlay in the build cache. Run `make generate` to go
back to the plain flight config (for D1s). An overlay build sets
`CONFIG_CONSOLE=y`, so `make check-console-disabled` fails on it.
`CONFIG_UART_CONSOLE` stays off.

RTT needs the `segger` west module, which `west.yml` allows. Run
`make zephyr` once after you pull this branch.

## Pick a bench

A bench file in `benches/` names every tty, probe serial, port and path. The
scripts never hard-code a device. A value that starts with `TODO` stops any
drill that needs it. An environment variable of the same name overrides a
value.

| File | Bench |
|---|---|
| `benches/pi500-dev.env` | Pi500 dev bench, user `proves` (the default). Its GRC runs GRC `main` (legacy LoRa), so profile drills fail there |
| `benches/pi500-ci.env` | Pi500 CI bench |

Set `<END>_PROBE_SERIAL=none` for a board with no debug probe. The gate then
reports that end's openocd `halted` check as N/A, not PASS. Set
`<END>_ELF_SHA256` to check that the ELF is the expected build.

The drills read counters from the GDS logs and need one openocd and one GDS
per end. Print the commands for a bench, then start each one:

```sh
python3 scripts/bench-drills/launch.py --bench scripts/bench-drills/benches/pi500-dev.env flight grc
```

OpenOCD always gets `adapter serial` and the bench's ports. The drills refuse
the default ports 3333, 4444 and 6666.

## Run a drill or the suite

```sh
cd scripts/bench-drills
python3 d1.py D1b --bench benches/pi500-dev.env
python3 run_all.py --bench benches/pi500-dev.env
python3 d5.py --param hours=6
```

Add `--dry-run` to run the drill logic offline against a fake bench on a
virtual clock. A dry run touches no device and is not evidence. Run the gate
tests with `python3 -m unittest test_gate test_rig test_ops test_d1`.

Exit status is 0 when every drill passes, 1 when one fails, and 2 when the
bench file cannot run it.

## Evidence

Each run writes `<LOG_ROOT>/<date>-drills/<time>-<drill>/` with
`evidence.md`, `transcript.log`, `measurements.json`, the GDS log lines from
the run, data-CDC captures and the USB topology. `LOG_ROOT` comes from
`--log-root`, then the bench file, then
`/Volumes/nvme-1tb-m4/proves/proves-gs-project/proves-core-reference/logs`.
Copy runs from the Pi500 to that NVMe path.

Post `evidence.md` as the results comment. A firmware or pin change on either
end invalidates every drill. A script-only fix reruns the drills that the
script touches.

## Known traps

The scripts encode these July findings:

- Flight counters arrive only in the Beacon (about 30 s). The drills wait 45 s before they read flight counters.
- GRC `BytesSent` lags. Byte parity uses the GRC data CDC as truth.
- The GRC event stream lags its channels. GRC commands are confirmed by channel movement.
- The GRC has no BootCount. A GRC reboot shows as openocd `halted` or `ConfigurationFailed`.
- Old dictionaries shift event ids. Each run checks that the dictionary's `projectVersion` is in the ELF under test.
- A killed flood replays its queued bytes into the next boot. The flood waits for the tty OUT queue to drain.
