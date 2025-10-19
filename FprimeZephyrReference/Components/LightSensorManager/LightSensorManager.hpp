// ======================================================================
// \title  LightSensorManager.hpp
// \author shirleydeng
// \brief  hpp file for LightSensorManager component implementation class
// ======================================================================

#ifndef LightSensor_LightSensorManager_HPP
#define LightSensor_LightSensorManager_HPP

#include "FprimeZephyrReference/Components/LightSensorManager/LightSensorManagerComponentAc.hpp"

namespace LightSensor {

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
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for loadSwitch
    //!
    //! Port for receiving call from load switch
    Drv::GpioStatus loadSwitch_handler(FwIndexType portNum,  //!< The port number
                                       Fw::Logic& state) override;

    //! Handler implementation for run
    //!
    //! Port receiving calls from the rate group
    //! (makes sense to me to be sync but not for some other examples)
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command RESET
    //!
    //! Command to turn the light sensor off and on
    void RESET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                          U32 cmdSeq            //!< The command sequence number
                          ) override;

    bool m_active = false;   //! Flag: if true then light sensing will occur else
                             //! no sensing will happen
};

}  // namespace LightSensor

#endif
