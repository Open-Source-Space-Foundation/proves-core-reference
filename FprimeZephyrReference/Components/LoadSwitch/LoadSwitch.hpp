// ======================================================================
// \title  LoadSwitch.hpp
// \author sarah
// \brief  hpp file for LoadSwitch component implementation class
// ======================================================================

#ifndef Components_LoadSwitch_HPP
#define Components_LoadSwitch_HPP

#include "FprimeZephyrReference/Components/LoadSwitch/LoadSwitchComponentAc.hpp"

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

    // ----------------------------------------------------------------------
    // Configuration Meant to be used in ***Topology.cpp
    // ----------------------------------------------------------------------

    void pin_configuration(const struct device* device, uint8_t pinNum);

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

    uint8_t m_pinNum;
    static const struct device* m_device;
};

}  // namespace Components

#endif
