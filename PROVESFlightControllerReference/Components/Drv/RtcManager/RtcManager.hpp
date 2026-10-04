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
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/timeutil.h>

namespace Drv {

class RtcManager final : public RtcManagerComponentBase {
  public:
    //! Result of readRtcSeconds()
    enum class RtcRead {
        OK,           //!< rtc_s is valid
        FAILED,       //!< Device not ready or rtc_get_time() failed
        IMPLAUSIBLE,  //!< Conversion failed or seconds outside years 2000 to 2099
    };

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

    //! Static C callback; forwards to update_callback_t() via user_data
    static void static_update_callback_t(const struct device* dev, void* user_data);

    //! Actual RTC update callback, corrects the time offset once per RTC second edge
    void update_callback_t();

    //! Read the RTC as epoch seconds. rc is the driver return code on FAILED
    RtcRead readRtcSeconds(std::int64_t& rtc_s, int& rc);

    //! Convert an rtc_time to epoch seconds. Returns false if out of range, with rtc_s set to -1 on ERANGE
    static bool rtcTimeToSeconds(const struct rtc_time& time_rtc, std::int64_t& rtc_s);

    //! Seed the time discipline from rtc_s at the current uptime, under the spinlock
    void seedDiscipline(std::int64_t rtc_s);

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
    U32 m_disciplineReadFaults;                     //!< Update callback only: failed or implausible reads
    U32 m_disciplineRejects;                        //!< Update callback only: REJECTED samples

    // rtc alarm members
    U16 m_curr_mask;               //!< The mask of the alarm present on hardware
    struct rtc_time m_alarm_time;  //!< Current alarm's time settings
};

}  // namespace Drv

#endif
