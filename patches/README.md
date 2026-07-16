# Patches Directory

This directory contains patches that are automatically applied to git submodules during the build process.

## fprime-gds-version.patch

This patch updates the `fprime-gds` version requirement in `lib/fprime/requirements.txt` from 4.1.0 to 4.1.1a2.

**Why:** The project requires fprime-gds 4.1.1a2 for specific features:
- file-uplink-cooldown argument
- file-uplink-chunk-size argument

The patch is automatically applied by the `make submodules` target to ensure version consistency and eliminate the version mismatch warning.

**Application:** This patch is applied automatically when running `make submodules` (or `make` which includes that target).

**Note:** After applying this patch, `git status` will show `lib/fprime` as modified. This is expected and should **not** be committed. The patched state is reapplied automatically on each `make submodules` run.

## S-Band HWIL fixes (2026-07-16, feat/sband-pr3-topology)

Two submodule fixes required for a working downlink; committed on local
submodule branches (unpushable — submodules track upstream remotes) and
exported here so the branch is reproducible:

- `fprime-comqueue-reprime-tolerance.patch` — apply in `lib/fprime`
  (`git apply ../../patches/fprime-comqueue-reprime-tolerance.patch`).
  ComQueue treats a redundant READY comStatus as a benign re-prime instead of
  asserting; required by ReferenceDeployment::primeDownlinkQueues(), without
  which the downlink deadlocks when the boot event storm drops the one-shot
  priming status. Upstream PR candidate (nasa/fprime).
- `zephyr-cdc-acm-pollmode-enable-drain.patch` — apply in
  `lib/zephyr-workspace/zephyr`. Drains poll-mode TX data queued before USB
  enumeration; without it CDC output can stay silent forever. Upstream PR
  candidate (zephyrproject-rtos/zephyr).
