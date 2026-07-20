#!/usr/bin/env bash
# Locate the Zephyr CDC ACM tty by USB VID:PID 0028:000f (set in the board
# defconfig); /dev/serial/by-id does not list this device on the integration
# runner. Prints the /dev/tty* path on stdout. Retries for ~20 s because the
# node can take a few seconds to (re-)enumerate after a power cycle, then
# exits 1 with diagnostics on stderr if the device never appears.
set -u

find_tty() {
    local d v p path ifn tty
    for d in /sys/bus/usb/devices/*/idVendor; do
        v=$(cat "$d" 2>/dev/null)
        p=$(cat "${d%idVendor}idProduct" 2>/dev/null)
        if [ "$v" = "0028" ] && [ "$p" = "000f" ]; then
            path=${d%/idVendor}
            for ifn in "$path"/*:*; do
                [ "$(cat "$ifn/bInterfaceClass" 2>/dev/null)" = "02" ] || continue
                tty=$(ls "$ifn/tty/" 2>/dev/null | head -1)
                if [ -n "$tty" ]; then
                    echo "/dev/$tty"
                    return 0
                fi
            done
        fi
    done
    return 1
}

for _attempt in $(seq 1 10); do
    if tty_path=$(find_tty); then
        echo "$tty_path"
        exit 0
    fi
    sleep 2
done

echo "ERROR: Zephyr CDC ACM (0028:000f) tty not found via sysfs" >&2
lsusb >&2 || true
exit 1
