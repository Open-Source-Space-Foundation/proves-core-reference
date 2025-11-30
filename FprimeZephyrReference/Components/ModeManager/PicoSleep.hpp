// ======================================================================
// \title  PicoSleep.hpp
// \brief  Wrapper for RP2350 dormant mode using AON Timer and POWMAN
// ======================================================================

#ifndef Components_PicoSleep_HPP
#define Components_PicoSleep_HPP

#include "Fw/Types/BasicTypes.hpp"

namespace Components {

/**
 * @brief Wrapper class for RP2350 Pico SDK dormant mode functionality
 *
 * The RP2350 uses the Always-On (AON) Timer and Power Manager (POWMAN)
 * for ultra-low-power dormant mode. Unlike the RP2040's RTC-based sleep,
 * the RP2350's AON timer runs from the Low Power Oscillator (LPOSC) which
 * stays active during dormant mode (~32kHz, less precise than XOSC).
 *
 * Power consumption in dormant mode:
 * - Dormant with AON timer: ~3mA
 * - POWMAN deep sleep: ~0.65-0.85mA
 *
 * IMPORTANT: On RP2350, sleepForSeconds() CAN return (unlike RP2040).
 * When the AON timer alarm fires, the processor wakes and execution
 * continues. If dormant entry fails or is disabled, falls back to
 * sys_reboot() for reliability.
 *
 * Known issue: RP2350 can halt after multiple dormant wake cycles
 * (see pico-sdk issue #2376). Set USE_DORMANT_MODE to false in
 * PicoSleep.cpp to use safer sys_reboot fallback.
 */
class PicoSleep {
  public:
    /**
     * @brief Enter dormant mode for specified duration
     *
     * Configures the AON timer alarm and enters RP2350 dormant mode.
     * The processor halts with only LPOSC running. When the alarm fires,
     * execution resumes and the function returns true.
     *
     * If dormant mode is not available or fails, falls back to sys_reboot()
     * which does not return.
     *
     * @param seconds Duration to sleep in seconds
     * @return true if woke from dormant successfully
     * @return false if dormant mode entry failed (native/sim builds)
     * @note On sys_reboot fallback, function does not return
     */
    static bool sleepForSeconds(U32 seconds);

    /**
     * @brief Check if dormant mode is supported on this platform
     *
     * @return true if AON timer dormant mode is available (RP2350)
     * @return false on native/sim builds or if USE_DORMANT_MODE is disabled
     */
    static bool isSupported();

  private:
    // Prevent instantiation
    PicoSleep() = delete;
};

}  // namespace Components

#endif
