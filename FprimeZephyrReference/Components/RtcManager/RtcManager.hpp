// ======================================================================
// \title  RtcManager.hpp
// \brief  hpp file for RtcManager component implementation class
// ======================================================================

#ifndef Components_RtcManager_HPP
#define Components_RtcManager_HPP

#include "FprimeZephyrReference/Components/RtcManager/RtcManagerComponentAc.hpp"

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
    void SET_TIME_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq,           //!< The command sequence number
                             Drv::TimeData t       //!< Set the time
                             ) override;

    //! Handler implementation for command GET_TIME
    //!
    //! GET_TIME command to get the time from the RTC
    //! Requirement RtcManager-002
    void GET_TIME_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq            //!< The command sequence number
                             ) override;
};

}  // namespace Components

#endif
