"""Fix the Zephyr SDK's macOS host-arch detection in setup.sh.

Observed on a self-hosted Apple Silicon CI runner: setup.sh derives the
download host string from bash's $HOSTTYPE, which reports 'x86_64' when
the shell itself is running translated under Rosetta -- even though the
hardware is arm64. That produces a toolchain download URL
(toolchain_gnu_macos-x86_64_...) for an asset that was never published
(only the macos-aarch64 asset exists), so the download 404s every time,
regardless of which downloader (wget/curl) or how many retries are used.

This patches the darwin host-detection branch to double check the true
hardware architecture via `sysctl hw.optional.arm64`, which reports the
physical CPU regardless of Rosetta translation.
"""

import sys

path = sys.argv[1]
content = open(path).read()

marker = "hw.optional.arm64"
if marker in content:
    print("⚠ Zephyr SDK macOS host-arch fix already applied")
    sys.exit(0)

original = """  darwin*)
    # Bash 3.x on AArch64 Darwin sets HOSTTYPE to 'arm64'
    if [ "${HOSTTYPE}" = "arm64" ]; then
      HOSTTYPE="aarch64"
    fi

    host="macos-${HOSTTYPE}"
    ;;"""

patched = """  darwin*)
    # Bash 3.x on AArch64 Darwin sets HOSTTYPE to 'arm64'
    if [ "${HOSTTYPE}" = "arm64" ]; then
      HOSTTYPE="aarch64"
    fi

    # A Rosetta-translated shell reports HOSTTYPE=x86_64 even on Apple
    # Silicon hardware; sysctl reports the true hardware architecture.
    if [ "${HOSTTYPE}" = "x86_64" ] && [ "$(sysctl -n hw.optional.arm64 2>/dev/null)" = "1" ]; then
      HOSTTYPE="aarch64"
    fi

    host="macos-${HOSTTYPE}"
    ;;"""

if original not in content:
    print(
        "❌ Error: darwin host-detection block not found in setup.sh — SDK setup.sh may have changed"
    )
    sys.exit(1)

open(path, "w").write(content.replace(original, patched))
print("✓ Applied Zephyr SDK macOS host-arch fix")
