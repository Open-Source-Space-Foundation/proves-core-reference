// native_sim has no RP2350 boot ROM: TRIGGER_BOOTLOADER succeeds and does nothing.
#pragma once
#include <stdint.h>

static inline void reset_usb_boot(uint32_t, uint32_t) {}
