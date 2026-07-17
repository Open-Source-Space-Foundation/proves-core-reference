#!/usr/bin/env python3
"""check_console_disabled.py:

Guard test for CI. Fails if the Zephyr UART console is enabled in the built
firmware configuration.

Background: the F' downlink and the Zephyr console share the same USB CDC ACM
device (cdc_acm_uart0). When the console is enabled, printk / Os::Console text
is interleaved with the binary CCSDS TM frames on that UART, which desyncs the
GDS deframer so no telemetry or command acks can be parsed. The console must
therefore stay disabled so the downlink UART carries frames only.

This inspects the generated Kconfig output (build-fprime-automatic-zephyr/
zephyr/.config) rather than prj.conf, so it also catches the console being
re-enabled via a board defconfig or a Kconfig default -- i.e. the config that
actually gets compiled.

Usage:
    python3 scripts/check_console_disabled.py [path/to/.config]

Exit status:
    0 - console is disabled (safe to merge)
    1 - console is enabled, or the .config was not found
"""

import sys
from pathlib import Path

DEFAULT_CONFIG = Path("build-fprime-automatic-zephyr/zephyr/.config")

# Symbols that, when set to y, put console text on the shared downlink UART.
# CONFIG_UART_CONSOLE is the direct cause; CONFIG_CONSOLE is its parent and is
# disabled by our fix, so we treat either being enabled as a regression.
FORBIDDEN_SYMBOLS = ("CONFIG_UART_CONSOLE", "CONFIG_CONSOLE")


def main(argv: list[str]) -> int:
    config_path = Path(argv[1]) if len(argv) > 1 else DEFAULT_CONFIG

    if not config_path.is_file():
        print(
            f"ERROR: {config_path} not found. Run 'make build' first so the "
            "generated Zephyr .config exists.",
            file=sys.stderr,
        )
        return 1

    enabled = []
    for line in config_path.read_text().splitlines():
        line = line.strip()
        for symbol in FORBIDDEN_SYMBOLS:
            if line == f"{symbol}=y":
                enabled.append(symbol)

    if enabled:
        print(
            "ERROR: the Zephyr console is enabled in the built firmware "
            f"({config_path}):",
            file=sys.stderr,
        )
        for symbol in enabled:
            print(f"    {symbol}=y", file=sys.stderr)
        print(
            "\nThe console shares cdc_acm_uart0 with the F' downlink. Console "
            "output interleaves with the CCSDS TM frames and desyncs the GDS "
            "deframer, so telemetry and command acks cannot be parsed.\n"
            "Keep CONFIG_CONSOLE and CONFIG_UART_CONSOLE disabled in prj.conf.",
            file=sys.stderr,
        )
        return 1

    print(
        f"OK: Zephyr console is disabled in {config_path} (downlink UART is frames-only)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
