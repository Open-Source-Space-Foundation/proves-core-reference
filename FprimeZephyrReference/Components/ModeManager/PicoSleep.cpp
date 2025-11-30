// ======================================================================
// \title  PicoSleep.cpp
// \brief  Implementation of RP2350 dormant mode wrapper
// ======================================================================

#include "FprimeZephyrReference/Components/ModeManager/PicoSleep.hpp"

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

namespace Components {

bool PicoSleep::sleepForSeconds(U32 seconds) {
    // Note: The actual RP2350 dormant mode implementation requires:
    // 1. Configure RTC alarm for the specified duration
    // 2. Configure POWMAN wake source to use RTC alarm
    // 3. Enter dormant mode via POWMAN peripheral
    //
    // For now, we use sys_reboot() to simulate the sleep/wake cycle.
    // The state file indicates we're in hibernation mode, so on reboot
    // the system will enter the wake window and listen for EXIT_HIBERNATION.
    //
    // TODO: Implement actual RP2350 dormant mode using POWMAN peripheral
    // when hardware testing is available.

    // The seconds parameter is stored in the persistent state and would be
    // used to configure the RTC alarm in a full implementation.
    (void)seconds;

#if defined(CONFIG_BOARD_NATIVE_POSIX) || defined(CONFIG_BOARD_NATIVE_SIM) || defined(CONFIG_ARCH_POSIX)
    // Native/simulation builds: Return false to exercise the failure handling path
    // This allows CI to test HibernationEntryFailed event emission, counter rollback,
    // and mode reversion to SAFE_MODE without actually rebooting.
    return false;
#else
    // Real hardware: Perform system reboot - the state file will indicate we're
    // in hibernation and on next boot, the system will enter the wake window
    sys_reboot(SYS_REBOOT_COLD);

    // Should never reach here on real hardware
    return false;
#endif
}

bool PicoSleep::isSupported() {
#if defined(CONFIG_BOARD_NATIVE_POSIX) || defined(CONFIG_BOARD_NATIVE_SIM) || defined(CONFIG_ARCH_POSIX)
    // Native/simulation: dormant mode is not supported (returns false to exercise failure path)
    return false;
#else
    // RP2350 supports dormant mode, but our implementation uses reboot as placeholder
    return true;
#endif
}

}  // namespace Components
