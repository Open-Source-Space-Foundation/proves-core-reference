// ======================================================================
// \title  LoadSwitch.hpp
// \author sarah
// \brief  hpp file for LoadSwitch component implementation class
// ======================================================================

#ifndef Components_LoadSwitch_HPP
#define Components_LoadSwitch_HPP

#include <zephyr/kernel.h>
#include "FprimeZephyrReference/Components/LoadSwitch/LoadSwitchComponentAc.hpp"

// Undefine EMPTY macro from Zephyr headers to avoid conflict with F Prime Os::Queue::EMPTY
#ifdef EMPTY
#undef EMPTY
#endif

// Forward declare Zephyr types to avoid header conflicts
struct device;

namespace Components {

class LoadSwitch final : public LoadSwitchComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct LoadSwitch object
    LoadSwitch(const char* const compName  //!< The component name
    );

    //! Destroy LoadSwitch object
    ~LoadSwitch();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TURN_ON
    void TURN_ON_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                            U32 cmdSeq            //!< The command sequence number
                            ) override;

    //! Handler implementation for command TURN_OFF
    void TURN_OFF_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq            //!< The command sequence number
                             ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for Reset
    void Reset_handler(FwIndexType portNum  //!< The port number
                       ) override;
};

}  // namespace Components

#endif
