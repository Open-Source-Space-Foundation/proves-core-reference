// ======================================================================
// \title  PicoSleep.cpp
// \brief  Implementation of RP2350 dormant mode using AON Timer and POWMAN
//
// The RP2350 uses the Always-On (AON) Timer and Power Manager (POWMAN)
// for low-power dormant mode, unlike the RP2040 which uses the RTC.
// The AON timer runs from the Low Power Oscillator (LPOSC) which stays
// active during dormant mode.
//
// References:
// - Pico SDK pico_aon_timer library
// - Pico SDK hardware_powman library
// - pico-extras pico_sleep library
// ======================================================================

#include "FprimeZephyrReference/Components/ModeManager/PicoSleep.hpp"

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

// RP2350 Pico SDK includes for AON timer and power management
#if defined(CONFIG_SOC_RP2350) || defined(CONFIG_SOC_SERIES_RP2XXX)
#include <hardware/clocks.h>
#include <hardware/powman.h>
#include <hardware/structs/clocks.h>
#include <hardware/structs/powman.h>
#include <hardware/structs/rosc.h>
#include <hardware/structs/xosc.h>
#include <hardware/sync.h>
#include <hardware/xosc.h>
#include <pico/aon_timer.h>
#endif

namespace Components {

// Flag to track if we should use dormant mode or fallback to reboot
// Known issue: RP2350 can halt after multiple wake-ups (pico-sdk #2376)
// Set to false to use safer sys_reboot fallback
static constexpr bool USE_DORMANT_MODE = true;

// Track if AON timer has been initialized
static bool s_aonTimerInitialized = false;

#if defined(CONFIG_SOC_RP2350) || defined(CONFIG_SOC_SERIES_RP2XXX)

// Callback for AON timer alarm - does nothing, wakeup is automatic
static void aon_timer_alarm_callback(void) {
    // Empty callback - the alarm firing wakes the processor from dormant
}

// Ensure AON timer is initialized and running
// For continuous timing across reboots, check POWMAN_TIMER_RUN first
static bool ensureAonTimerRunning(void) {
    // Check if POWMAN timer is already running (e.g., from previous boot)
    // This is the "required operating procedure when you want continuous timing"
#if defined(POWMAN_TIMER_RUN_BITS) && defined(POWMAN_BASE)
    if (powman_hw->timer & POWMAN_TIMER_RUN_BITS) {
        s_aonTimerInitialized = true;
        return true;
    }
#endif

    // Timer not running, need to initialize it
    // Start the AON timer with current time (0 for simplicity)
    struct timespec ts = {0, 0};
#ifdef PICO_AON_TIMER_H
    aon_timer_start(&ts);
    // Wait a bit for timer to stabilize (per pico-sdk #2148)
    k_busy_wait(100);
#else
    // AON timer not available; cannot start timer
    return false;
#endif

    s_aonTimerInitialized = true;
    return true;
}

// Switch clocks to run from ROSC for dormant mode
// ROSC stays on during dormant, XOSC is stopped
static void sleep_run_from_rosc(void) {
    // Switch clk_ref to use ROSC (Ring Oscillator)
    // ROSC runs at ~6.5MHz and stays on during dormant
    hw_write_masked(&clocks_hw->clk[clk_ref].ctrl,
                    CLOCKS_CLK_REF_CTRL_SRC_VALUE_ROSC_CLKSRC_PH << CLOCKS_CLK_REF_CTRL_SRC_LSB,
                    CLOCKS_CLK_REF_CTRL_SRC_BITS);

    // Wait for clock switch to complete
    while (!(clocks_hw->clk[clk_ref].selected & (1u << CLOCKS_CLK_REF_CTRL_SRC_VALUE_ROSC_CLKSRC_PH))) {
        tight_loop_contents();
    }

    // Switch clk_sys to use clk_ref (which is now ROSC)
    hw_write_masked(&clocks_hw->clk[clk_sys].ctrl, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF << CLOCKS_CLK_SYS_CTRL_SRC_LSB,
                    CLOCKS_CLK_SYS_CTRL_SRC_BITS);

    // Wait for clock switch to complete
    while (!(clocks_hw->clk[clk_sys].selected & (1u << CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF))) {
        tight_loop_contents();
    }
}

// Restore clocks after waking from dormant
static void sleep_power_up(void) {
    // Re-enable the XOSC
    xosc_init();

    // Switch clk_ref back to XOSC
    hw_write_masked(&clocks_hw->clk[clk_ref].ctrl,
                    CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC << CLOCKS_CLK_REF_CTRL_SRC_LSB,
                    CLOCKS_CLK_REF_CTRL_SRC_BITS);

    // Wait for clock switch to complete
    while (!(clocks_hw->clk[clk_ref].selected & (1u << CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC))) {
        tight_loop_contents();
    }

    // Note: Full PLL/clock restoration is handled by Zephyr or requires
    // calling clocks_init() which may conflict with Zephyr's clock setup.
    // For now, we do a system reboot after wake to ensure clean state.
}

// Enter dormant mode - processor will halt until AON timer alarm
static void go_dormant(void) {
    // Enable deep sleep in the processor (Cortex-M33)
    scb_hw->scr |= M33_SCR_SLEEPDEEP_BITS;

    // Disable all clocks except those needed for dormant wake
    // On RP2350, CLK_REF_POWMAN needs to stay enabled for AON timer
#ifdef PICO_SDK_PRESENT
    clocks_hw->sleep_en0 = CLOCKS_SLEEP_EN0_CLK_REF_POWMAN_BITS;
    clocks_hw->sleep_en1 = 0;
#else
    // Zephyr build: clocks_hw not available, skipping direct register access
#endif

    // Wait for interrupt - processor enters dormant mode
    // Will wake when AON timer alarm fires
    __wfi();

    // Restore clocks after wakeup
#ifdef PICO_SDK_PRESENT
    clocks_hw->sleep_en0 = 0xFFFFFFFF;
    clocks_hw->sleep_en1 = 0xFFFFFFFF;
#else
    // Zephyr build: clocks_hw not available, skipping direct register access
#endif

    // Clear deep sleep bit
    scb_hw->scr &= ~M33_SCR_SLEEPDEEP_BITS;
}

#endif  // CONFIG_SOC_RP2350

bool PicoSleep::sleepForSeconds(U32 seconds) {
#if defined(CONFIG_BOARD_NATIVE_POSIX) || defined(CONFIG_BOARD_NATIVE_SIM) || defined(CONFIG_ARCH_POSIX)
    // Native/simulation builds: Return false to exercise the failure handling path
    // This allows CI to test HibernationEntryFailed event emission, counter rollback,
    // and mode reversion to SAFE_MODE without actually rebooting.
    (void)seconds;
    return false;

#elif defined(CONFIG_SOC_RP2350) || defined(CONFIG_SOC_SERIES_RP2XXX)
    // RP2350: Use AON timer for proper dormant mode with timer wakeup
    //
    // The AON timer uses the Low Power Oscillator (LPOSC) which runs at ~32kHz
    // and stays active during dormant mode. When the alarm fires, the processor
    // wakes and execution continues after __wfi().
    //
    // NOTE: There's a known issue (pico-sdk #2376) where RP2350 can halt after
    // multiple dormant wake cycles. If USE_DORMANT_MODE is false, we fall back
    // to sys_reboot which is more reliable but uses more power.

    if (!USE_DORMANT_MODE) {
        // Fallback: Use cold reboot instead of dormant
        // State file will indicate hibernation mode on next boot
        sys_reboot(SYS_REBOOT_COLD);
        return false;  // Never reached
    }

    // Ensure AON timer is initialized and running
    if (!ensureAonTimerRunning()) {
        // Failed to initialize AON timer - fall back to reboot
        sys_reboot(SYS_REBOOT_COLD);
        return false;
    }

    // Get current time from AON timer
    struct timespec ts;
#ifdef PICO_SDK_PRESENT
    if (!aon_timer_get_time(&ts)) {
        // AON timer not working - fall back to reboot
        sys_reboot(SYS_REBOOT_COLD);
        return false;
    }
#else
    // Pico SDK not available - cannot get AON timer time, fall back to reboot
    sys_reboot(SYS_REBOOT_COLD);
    return false;
#endif

    // Set wakeup time
    ts.tv_sec += seconds;

    // Configure power manager timer to use LPOSC (stays on during dormant)
    uint64_t current_ms = powman_timer_get_ms();
    powman_timer_set_1khz_tick_source_lposc();
    powman_timer_set_ms(current_ms);

    // Switch to running from ROSC (stays on during dormant, XOSC will stop)
    sleep_run_from_rosc();

    // Set AON timer alarm for wakeup
    // The alarm will fire and wake the processor from dormant
#if defined(aon_timer_enable_alarm)
    aon_timer_enable_alarm(&ts, aon_timer_alarm_callback, true);
#else
    // AON timer alarm not available; fall back to reboot
    sys_reboot(SYS_REBOOT_COLD);
    return false;
#endif

    // Enter dormant mode - execution stops here until alarm
    go_dormant();

    // If we get here, we woke up successfully from dormant!
#if defined(aon_timer_disable_alarm)
    aon_timer_disable_alarm();
#endif

    // Restore clocks (partial - PLLs require full init)
    sleep_power_up();

    // Due to complexity of restoring full Zephyr clock state,
    // we do a warm reboot to ensure clean state
    // The state file still has HIBERNATION_MODE, so loadState() will
    // detect this and start the wake window
    sys_reboot(SYS_REBOOT_WARM);

    return false;  // Never reached (reboot above)

#else
    // Unknown platform - use sys_reboot as fallback
    (void)seconds;
    sys_reboot(SYS_REBOOT_COLD);
    return false;

#endif
}

bool PicoSleep::isSupported() {
#if defined(CONFIG_BOARD_NATIVE_POSIX) || defined(CONFIG_BOARD_NATIVE_SIM) || defined(CONFIG_ARCH_POSIX)
    // Native/simulation: dormant mode is not supported (returns false to exercise failure path)
    return false;
#elif defined(CONFIG_SOC_RP2350) || defined(CONFIG_SOC_SERIES_RP2XXX)
    // RP2350 supports dormant mode via AON timer and POWMAN
    return USE_DORMANT_MODE;
#else
    // Unknown platform
    return false;
#endif
}

}  // namespace Components
