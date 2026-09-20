# Patches Directory

This directory holds one patch: `fprime-yamcs-noapp-path.patch`.

It patches the pip-installed `fprime-yamcs` package's `fprime_yamcs/__main__.py`,
fixing `--no-app` path handling: `parsed_args.dictionary` is passed as a `Path`
where the callee expects one, and the venv `bin` directory is prepended to `PATH`
so the Java subprocesses can find the fprime-yamcs console scripts.

**Application:** applied automatically by `make fprime-venv` (and therefore by
`make`); skipped if already applied.

`*.patch` files keep trailing whitespace: pre-commit excludes `patches/` because
the patch context must byte-match the file it patches.

## Former patches

The following module patches were migrated to `Open-Source-Space-Foundation` fork
integration branches pinned in `west.yml` and `.gitmodules`.

| Former patch | Module | Integration PR |
|---|---|---|
| 0001 RF-switch GPIO, 0002 Zephyr-4.3 Kconfig, 0003 LR_FHSS path, 0006 wakeup settle, 0008 RAC mutex, 0010 board.yml schema | usp_zephyr | Open-Source-Space-Foundation/usp_zephyr#7 |
| 0009 radio-planner failsafe unlock exemption | usp | Open-Source-Space-Foundation/usp#3 |
| 0005 + 0007 CDC-ACM TX fixes | zephyr | Open-Source-Space-Foundation/zephyr#3 |
| fprime-com-aggregator-bounded-timeout, fprime-sched-tick-drop | fprime | Open-Source-Space-Foundation/fprime#5 |
