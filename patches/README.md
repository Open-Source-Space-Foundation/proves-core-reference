# Patches Directory

This directory contains patches that are automatically applied to git submodules during the build process.

## fprime-tlmpacketizer-i16-offset.patch

Shrinks `Svc::TlmPacketizer`'s per-channel `packetOffset` array from `FwSignedSizeType`
(64-bit) to `I16` in `lib/fprime`. The offset is a byte index into a ComBuffer-sized
packet (or the -1 "not in this packet" sentinel), so I16 is ample; the range `FW_ASSERT`
in `setPacketList` is tightened to the I16 max.

**Why:** With this project's `MAX_PACKETIZER_CHANNELS=202` and `MAX_PACKETIZER_PACKETS=22`,
the dense channels×packets offset matrix dominates the component: the patch shrinks the
`tlmSend` instance from 60,480 to 33,008 B of BSS (−27,472 B), which flows 1:1 into the
libc malloc arena (free-at-boot measured 11,504 → 38,976 B on a v5e board).

**Status:** Upstream candidate for nasa/fprime; carried here until accepted.

**Application:** Applied automatically by `make submodules` with a reverse-apply
idempotency check. As with the other fprime patches, `lib/fprime` will show as modified —
do not commit the submodule pointer.

## fprime-gds-version.patch

This patch updates the `fprime-gds` version requirement in `lib/fprime/requirements.txt` from 4.1.0 to 4.1.1a2.

**Why:** The project requires fprime-gds 4.1.1a2 for specific features:
- file-uplink-cooldown argument
- file-uplink-chunk-size argument

The patch is automatically applied by the `make submodules` target to ensure version consistency and eliminate the version mismatch warning.

**Application:** This patch is applied automatically when running `make submodules` (or `make` which includes that target).

**Note:** After applying this patch, `git status` will show `lib/fprime` as modified. This is expected and should **not** be committed. The patched state is reapplied automatically on each `make submodules` run.
