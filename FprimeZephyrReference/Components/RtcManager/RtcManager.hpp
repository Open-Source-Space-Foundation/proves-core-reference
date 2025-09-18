// ======================================================================
// \title  RtcManager.hpp
// \brief  hpp file for RtcManager component implementation class
// ======================================================================

#ifndef Components_RtcManager_HPP
#define Components_RtcManager_HPP

#include "FprimeZephyrReference/Components/RtcManager/RtcManagerComponentAc.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/timeutil.h>

namespace Components {

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

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SET_TIME
    //!
    //! SET_TIME command to set the time on the RTC
    //! Requirement RtcManager-001
    void SET_TIME_cmdHandler(FwOpcodeType opCode,                //!< The opcode
                             U32 cmdSeq,                         //!< The command sequence number
                             Components::RtcManager_TimeInput t  //!< Set the time
                             ) override;

    //! Handler implementation for command GET_TIME
    //!
    //! GET_TIME command to get the time from the RTC
    //! Requirement RtcManager-002
    void GET_TIME_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq            //!< The command sequence number
                             ) override;

    void timeGetPort_handler(FwIndexType portNum, /*!< The port number*/
                             Fw::Time& time       /*!< The U32 cmd argument*/
    );

    //! Zephyr device stores the initialized RV2038 sensor
    const struct device* rv3028;
};

}  // namespace Components

#endif
