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

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Log RTC not ready once until throttle is cleared
    void log_LOG_RtcNotReady();

    //! Clear RTC not ready log throttle
    void log_LOG_RtcNotReady_ThrottleClear();

    //! Log RTC get time failure once until throttle is cleared
    void log_LOG_RtcGetTimeFailed(int rc);

    //! Clear RTC get time failure log throttle
    void log_LOG_RtcGetTimeFailed_ThrottleClear();

    //! Log RTC invalid time once until throttle is cleared
    void log_LOG_RtcInvalidTime();

    //! Clear RTC invalid time log throttle
    void log_LOG_RtcInvalidTime_ThrottleClear();

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
};

}  // namespace Drv

#endif
