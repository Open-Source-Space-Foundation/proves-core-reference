# Patches Directory

This directory once carried a stack of module patches (usp_zephyr, usp, zephyr,
fprime) applied at build time. As of 2026-07-26 all of those fixes have been
migrated to the `Open-Source-Space-Foundation` fork integration branches
(`feat/proves-usp-radio`), which are pinned directly in `west.yml` and
`.gitmodules`. The former patch-apply Makefile targets (`usp-patches`,
`usp-core-patches`, `zephyr-patches`, and the fprime steps in `submodules`)
were removed with them.

Where the removed patches live now (integration PRs, each linking its
constituent PRs):

| Former patch | Module | Integration PR |
|---|---|---|
| 0001 RF-switch GPIO, 0002 Zephyr-4.3 Kconfig, 0003 LR_FHSS path, 0006 wakeup settle, 0008 RAC mutex, 0010 board.yml schema | usp_zephyr | Open-Source-Space-Foundation/usp_zephyr#7 |
| 0009 radio-planner failsafe unlock exemption | usp | Open-Source-Space-Foundation/usp#3 |
| 0005 + 0007 CDC-ACM TX fixes | zephyr | Open-Source-Space-Foundation/zephyr#3 |
| fprime-com-aggregator-bounded-timeout, fprime-sched-tick-drop | fprime | Open-Source-Space-Foundation/fprime#5 |

## fprime-yamcs-noapp-path.patch (the one remaining patch)

Patches the *pip-installed* `fprime-yamcs` package (not a git submodule), so it
cannot move to a fork pin and remains a carried patch. It fixes the `--no-app`
path handling in `fprime_yamcs/__main__.py`.

**Application:** applied automatically by `make fprime-venv` (and therefore by
`make`), alongside the scripted fprime-yamcs fixes in `tools/`.
