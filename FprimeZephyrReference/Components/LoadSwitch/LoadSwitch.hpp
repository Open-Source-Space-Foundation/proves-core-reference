// ======================================================================
// \title  LoadSwitch.hpp
// \author Moises, sarah
// \brief  hpp file for LoadSwitch component implementation class
// ======================================================================

#ifndef Components_LoadSwitch_HPP
#define Components_LoadSwitch_HPP

#include "FprimeZephyrReference/Components/LoadSwitch/LoadSwitchComponentAc.hpp"
#include <zephyr/kernel.h>

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

    //! Handler implementation for loadSwitchStateGet
    //!
    //! Input port to get the state of the load switch (called by other components)
    Fw::On loadSwitchStateGet_handler(FwIndexType portNum  //!< The port number
                                      ) override;

    //! Handler implementation for turnOn
    void turnOn_handler(FwIndexType portNum  //!< The port number
                        ) override;

    //! Handler implementation for turnOff
    void turnOff_handler(FwIndexType portNum  //!< The port number
                         ) override;

    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Set the load switch state (common implementation for commands and ports)
    void setLoadSwitchState(Fw::On state  //!< The desired state (ON or OFF)
    );

    //! Get current load switch state
    Fw::On getLoadSwitchState();  //<! Get the current state (ON or OFF)

    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------
    Fw::Time m_on_timeout;  //!< Time when load switch was turned on plus a delay time
};

}  // namespace Components

#endif
