// ======================================================================
// \title  RtcManager.hpp
// \brief  hpp file for RtcManager component implementation class
// ======================================================================

#ifndef Components_RtcManager_HPP
#define Components_RtcManager_HPP

#include <Fw/Logger/Logger.hpp>
#include <atomic>
#include <cerrno>

#include "FprimeZephyrReference/Components/Drv/RtcManager/RtcManagerComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/clock.h>
#include <zephyr/sys/timeutil.h>

namespace Drv {

class RtcManager final : public RtcManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct RtcManager object
    RtcManager(const char* const compName  //!< The component name
    );

    //! Destroy RtcManager object
    ~RtcManager();

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Configure the RTC device
    void configure(const struct device* dev);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for timeGetPort
    //!
    //! Port to retrieve time
    void timeGetPort_handler(FwIndexType portNum,  //!< The port number
                             Fw::Time& time        //!< Reference to Time object
                             ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TIME_SET
    //!
    //! TIME_SET command to set the time on the RTC
    void TIME_SET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq,           //!< The command sequence number
                             Drv::TimeData t       //!< Set the time
                             ) override;

    //! Handler implementation for command TEST_UNCONFIGURE_DEVICE
    //!
    //! TEST_UNCONFIGURE_DEVICE command to unconfigure the RTC device. Used for testing RTC failover to monotonic time
    //! since boot.
    void TEST_UNCONFIGURE_DEVICE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                            U32 cmdSeq            //!< The command sequence number
                                            ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Rescales useconds to ensure monotonic increase because RV3028 does not provide sub-second precision
    //! We have two clocks: the RTC clock which provides seconds, and the system uptime clock which provides
    //! milliseconds since boot. We use the initial offset of the system uptime clock when the RTC time is first read
    //! to ensure that the microseconds portion of the time is always increasing.
    U32 rescaleUseconds(U32 current_seconds, U32 current_useconds);

    //! Validate time data
    bool timeDataIsValid(Drv::TimeData t);

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    std::atomic<bool> m_console_throttled;  //!< Counter for console throttle
    const struct device* m_dev;             //!< The initialized Zephyr RTC device
    U32 m_last_seen_seconds = 0;            //!< The last seen seconds value from the RTC
    // U32 m_last_seconds_with_scaled_microseconds = 0;  //!< The last seconds value when microseconds were scaled
    U32 m_useconds_offset = 0;  //!< The offset to apply to microseconds to ensure monotonicity
};

}  // namespace Drv

#endif
