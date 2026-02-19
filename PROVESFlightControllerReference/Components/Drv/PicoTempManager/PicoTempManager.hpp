// ======================================================================
// \title  PicoTempManager.hpp
// \author jcowley04
// \brief  hpp file for PicoTempManager component implementation class
// ======================================================================

#ifndef Drv_PicoTempManager_HPP
#define Drv_PicoTempManager_HPP

#include "PROVESFlightControllerReference/Components/Drv/PicoTempManager/PicoTempManagerComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Drv {

class PicoTempManager final : public PicoTempManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PicoTempManager object
    PicoTempManager(const char* const compName  //!< The component name
    );

    //! Destroy PicoTempManager object
    ~PicoTempManager();

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Configure the die_temp device
    void configure(const struct device* dev);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Run loop
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command GetTemperature
    //!
    //! Command to get the temperature in degrees Celsius
    void GetPicoTemperature_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                       U32 cmdSeq            //!< The command sequence number
                                       ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Initialize the die_temp device
    Fw::Success initializeDevice();

    //! Deinitialize the die_temp device
    Fw::Success deinitializeDevice();

    //! Check if the die_temp device is initialized
    bool isDeviceInitialized();

    //! Get the temperature in degrees Celsius from the die_temp device
    F64 getPicoTemperature(Fw::Success& condition);

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized die_temp sensor
    const struct device* m_dev;
};

}  // namespace Drv

#endif
