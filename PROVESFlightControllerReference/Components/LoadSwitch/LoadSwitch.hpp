// ======================================================================
// \title  LoadSwitch.hpp
// \author Moises, sarah
// \brief  hpp file for LoadSwitch component implementation class
// ======================================================================

#ifndef Components_LoadSwitch_HPP
#define Components_LoadSwitch_HPP

#include "PROVESFlightControllerReference/Components/LoadSwitch/LoadSwitchComponentAc.hpp"

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

    //! Handler implementation for command GET_IS_ON
    void GET_IS_ON_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                              U32 cmdSeq            //!< The command sequence number
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
    //! \return true when the switch reads back in the requested state
    bool setLoadSwitchState(Fw::On state  //!< The desired state (ON or OFF)
    );

    //! Write the switch GPIO and read it back
    //! \return true when the switch reads back in the requested state
    bool writeLoadSwitchState(Fw::On state  //!< The desired state (ON or OFF)
    );

    //! Read the current load switch state
    //! \return false when the GPIO could not be read
    bool readLoadSwitchState(Fw::On& state  //!< The current state (ON or OFF)
    );

    //! State last sent to the loadSwitchStateChanged ports. The face managers start OFF.
    Fw::On m_notifiedState = Fw::On::OFF;
};

}  // namespace Components

#endif
