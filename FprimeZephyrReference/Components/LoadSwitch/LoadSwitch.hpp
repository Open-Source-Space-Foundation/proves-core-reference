// ======================================================================
// \title  LoadSwitch.hpp
// \author kyang25
// \brief  hpp file for LoadSwitch component implementation class
// ======================================================================

#ifndef Components_LoadSwitch_HPP
#define Components_LoadSwitch_HPP

#include "FprimeZephyrReference/Components/LoadSwitch/LoadSwitchComponentAc.hpp"

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

    //! Handler implementation for command TODO
    //!
    //! TODO
    void TODO_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                         U32 cmdSeq            //!< The command sequence number
                         ) override;
};

}  // namespace Components

#endif
