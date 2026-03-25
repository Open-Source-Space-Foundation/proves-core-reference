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

    //! Validate time data
    bool timeDataIsValid(Drv::TimeData t);

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    std::atomic<bool> m_console_throttled;  //!< Counter for console throttle
    const struct device* m_dev;             //!< The initialized Zephyr RTC device
    RtcHelper m_rtcHelper;                  //!< Helper for RTC operations

    // rtc alarm members

    U16 curr_alarm_id;             //!< The ID of the alarm present on hardware
    U16 curr_mask;                 //!< The mask of the alarm present on hardware
    struct rtc_time m_alarm_time;  //!< Current alarm's time settings
    bool alarm_set;                //!< Alarm present on hardware or not
};

}  // namespace Drv

#endif
