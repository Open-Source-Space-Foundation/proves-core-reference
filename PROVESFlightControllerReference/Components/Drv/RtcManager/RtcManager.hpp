// ======================================================================
// \title  RtcManager.hpp
// \brief  hpp file for RtcManager component implementation class
// ======================================================================

#ifndef Components_RtcManager_HPP
#define Components_RtcManager_HPP

#include <Fw/Logger/Logger.hpp>
#include <atomic>
#include <cerrno>

#include "PROVESFlightControllerReference/Components/Drv/RtcManager/RtcHelper.hpp"
#include "PROVESFlightControllerReference/Components/Drv/RtcManager/RtcManagerComponentAc.hpp"
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
    //!
    //! WARNING: This method is in a critical path for FPrime to get time.
    //! NOTE: Events require time therefore we only log to console in this method.
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

    //! Handler implementation for command ALARM_SET
    //!
    //! ALARM_SET command to set an alarm on the RTC
    void ALARM_SET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                              U32 cmdSeq,           //!< The command sequence number
                              Drv::TimeData t       //!< Time to set the alarm for
                              ) override;

    //! Handler implementation for command ALARM_CANCEL
    //!
    //! ALARM_CANCEL command to cancel any set alarms on the RTC
    void ALARM_CANCEL_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                 U32 cmdSeq,           //!< The command sequence number
                                 U16 ID                //!< ID of the alarm to cancel
                                 ) override;

    //! Handler implementation for command ALARM_LIST
    //!
    //! ALARM_LIST command to list all set alarms on the RTC
    void ALARM_LIST_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq            //!< The command sequence number
                               ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Alarm callback kicker method. Must be static but cannot reference this in a static context
    static void static_alarm_callback_t(const struct device* dev, uint16_t id, void* user_data);

    //! Actual alarm callback, for triggering events
    void alarm_callback_t(const struct device* dev, uint16_t id);

    //! Log RTC not ready once until throttle is cleared
    void log_CONSOLE_RtcNotReady();

    //! Clear RTC not ready log throttle
    void log_CONSOLE_RtcNotReady_ThrottleClear();

    //! Log RTC get time failure once until throttle is cleared
    void log_CONSOLE_RtcGetTimeFailed(int rc);

    //! Clear RTC get time failure log throttle
    void log_CONSOLE_RtcGetTimeFailed_ThrottleClear();

    //! Log RTC invalid time once until throttle is cleared
    void log_CONSOLE_RtcInvalidTime();

    //! Clear RTC invalid time log throttle
    void log_CONSOLE_RtcInvalidTime_ThrottleClear();

    //! Validate time data
    bool timeDataIsValid(Drv::TimeData t);

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    const struct device* m_dev;                    //!< The initialized Zephyr RTC device
    RtcHelper m_rtcHelper;                         //!< Helper for RTC operations
    std::atomic<bool> m_RtcNotReadyThrottle;       //!< Throttle for RtcNotReady
    std::atomic<bool> m_RtcGetTimeFailedThrottle;  //!< Throttle for RtcGetTimeFailed
    std::atomic<bool> m_RtcInvalidTimeThrottle;    //!< Throttle for RtcInvalidTime

    // rtc alarm members
    U16 m_curr_mask;               //!< The mask of the alarm present on hardware
    struct rtc_time m_alarm_time;  //!< Current alarm's time settings
};

}  // namespace Drv

#endif
