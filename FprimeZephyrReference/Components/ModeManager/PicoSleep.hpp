// ======================================================================
// \title  PicoSleep.hpp
// \brief  Wrapper for RP2350 dormant mode using RTC alarm wake
// ======================================================================

#ifndef Components_PicoSleep_HPP
#define Components_PicoSleep_HPP

#include "Fw/Types/BasicTypes.hpp"

namespace Components {

/**
 * @brief Wrapper class for RP2350 Pico SDK dormant mode functionality
 *
 * The RP2350's dormant mode puts the chip into an ultra-low-power state
 * where only the RTC continues running. When the RTC alarm fires, the
 * system reboots (not resumes).
 *
 * IMPORTANT: sleepForSeconds() does NOT return. The system will reboot
 * when the RTC alarm fires. State must be persisted before calling.
 */
class PicoSleep {
  public:
    /**
     * @brief Enter dormant mode for specified duration
     *
     * Configures the RTC alarm and enters RP2350 dormant mode.
     * This function does NOT return - the system will reboot when
     * the RTC alarm fires.
     *
     * @param seconds Duration to sleep in seconds
     * @return false if dormant mode entry failed (function returns),
     *         true is never returned (system reboots on success)
     */
    static bool sleepForSeconds(U32 seconds);

    /**
     * @brief Check if dormant mode is supported on this platform
     *
     * @return true if dormant mode is available
     */
    static bool isSupported();

  private:
    // Prevent instantiation
    PicoSleep() = delete;
};

}  // namespace Components

#endif
