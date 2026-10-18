#!/bin/bash
# Dev-bench only. backup: dump bootloader+slot0+slot1 (0x10000000, 3 MB) to a file.
# restore: write that file back and verify by re-dumping. Storage partition is never touched.
# Usage: backup-restore.sh backup <file> | restore <file>
set -euo pipefail
OCD() { ~/openocd/src/openocd -s ~/openocd/tcl -f interface/cmsis-dap.cfg -c "adapter serial E6647C74034E602F" -f target/rp2350.cfg -c "adapter speed 5000" "$@"; }
case "${1:?backup|restore}" in
  backup)
    OCD -c init -c "reset halt" -c "dump_image $2 0x10000000 0x300000" -c "reset run" -c shutdown 2>&1 | grep -E "dumped|Error" ;;
  restore)
    OCD -c init -c "reset halt" -c "flash write_image erase $2 0x10000000" -c "verify_image $2 0x10000000" -c "reset run" -c shutdown 2>&1 | grep -E "wrote|verified|Error|error" ;;
esac
