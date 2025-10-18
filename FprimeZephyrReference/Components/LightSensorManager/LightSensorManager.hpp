// ======================================================================
// \title  LightSensorManager.hpp
// \author adriano
// \brief  hpp file for LightSensorManager component implementation class
// ======================================================================

#ifndef Components_LightSensorManager_HPP
#define Components_LightSensorManager_HPP

#include "FprimeZephyrReference/Components/LightSensorManager/LightSensorManagerComponentAc.hpp"

namespace Components {

class LightSensorManager final : public LightSensorManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct LightSensorManager object
    LightSensorManager(const char* const compName  //!< The component name
    );

    //! Destroy LightSensorManager object
    ~LightSensorManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command RESET
    void RESET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                          U32 cmdSeq            //!< The command sequence number
                          ) override;


    Fw::On m_state = Fw::On::OFF; //! Keeps track if sensor is on or off
};

}  // namespace Components

#endif
