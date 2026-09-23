// ======================================================================
// \title  RtcManager.hpp
// \brief  hpp file for RtcManager component implementation class
// ======================================================================

#ifndef Components_RtcManager_HPP
#define Components_RtcManager_HPP

#include <Fw/Logger/Logger.hpp>
#include <atomic>
#include <cerrno>

#include "PROVESFlightControllerReference/Components/Drv/RtcManager/RtcManagerComponentAc.hpp"
#include "PROVESFlightControllerReference/Components/Drv/RtcManager/TimeDiscipline.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
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
    //! WARNING: This method is in a critical path for FPrime to get time. It must never call into
    //! the eventing system: no event ports, commands, or telemetry. It does not access the RTC
    //! hardware; it only uses uptime and the time offset held by TimeDiscipline.
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
    void TIME_SET_cmdHandler(FwOpcodeType opCode,    //!< The opcode
                             U32 cmdSeq,             //!< The command sequence number
                             const Drv::TimeData& t  //!< Set the time
                             ) override;

    //! Handler implementation for command ALARM_SET
    //!
    //! ALARM_SET command to set an alarm on the RTC
    void ALARM_SET_cmdHandler(FwOpcodeType opCode,    //!< The opcode
                              U32 cmdSeq,             //!< The command sequence number
                              const Drv::TimeData& t  //!< Time to set the alarm for
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

    //! Parameter update method. Runs when parameter for timebase is changed and cancels all running sequences to avoid
    //! conflict
    void parameterUpdated(FwPrmIdType id) override;

    //! Alarm callback kicker method. Must be static but cannot reference this in a static context
    static void static_alarm_callback_t(const struct device* dev, uint16_t id, void* user_data);

    //! Actual alarm callback, for triggering events
    void alarm_callback_t(const struct device* dev, uint16_t id);

    //! RTC update callback kicker method. Must be static but cannot reference this in a static context.
    //! Runs on the system workqueue thread, not the timeGetPort caller's thread.
    static void static_update_callback_t(const struct device* dev, void* user_data);

    //! Actual RTC update callback. Runs on the system workqueue thread once per RTC second edge.
    //! May emit telemetry and events, but only after releasing the spinlock.
    void update_callback_t();

    //! Read the RTC and convert to epoch seconds via timeutil_timegm(). Returns false on a
    //! nonzero rtc_get_time() return code or an out-of-range (ERANGE) conversion.
    bool readRtcSeconds(std::int64_t& rtc_s);

    //! Current uptime in microseconds, from k_uptime_ticks()
    static std::int64_t uptimeUs();

    //! Log RTC not disciplined once until throttle is cleared
    void log_CONSOLE_RtcNotDisciplined();

    //! Clear RTC not disciplined log throttle
    void log_CONSOLE_RtcNotDisciplined_ThrottleClear();

    //! Validate time data
    bool timeDataIsValid(Drv::TimeData t);

    //! Disarm the RTC alarm: unregister the callback (which also disables the
    //! alarm interrupt), write the disabled alarm (mask 0), and clear a stale
    //! alarm flag (AF) with rtc_alarm_is_pending(). Writing the disabled alarm
    //! can set AF on the RV3028; disabling the interrupt first prevents a false
    //! AlarmTriggered. Returns the first negative return code encountered, or
    //! the non-negative result of rtc_alarm_is_pending() on success.
    int disarmAlarm();

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    const struct device* m_dev;                     //!< The initialized Zephyr RTC device
    struct k_spinlock m_lock;                       //!< Guards m_discipline
    TimeDiscipline m_discipline;                    //!< Disciplines uptime + time offset against the RTC
    std::atomic<bool> m_RtcNotDisciplinedThrottle;  //!< Throttle for RtcNotDisciplined

    // rtc alarm members
    U16 m_curr_mask;               //!< The mask of the alarm present on hardware
    struct rtc_time m_alarm_time;  //!< Current alarm's time settings
};

}  // namespace Drv

#endif
