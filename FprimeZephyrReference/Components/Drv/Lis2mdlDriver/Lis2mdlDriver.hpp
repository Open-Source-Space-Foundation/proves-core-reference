// ======================================================================
// \title  Lis2mdlDriver.hpp
// \brief  hpp file for Lis2mdlDriver component implementation class
// ======================================================================

#ifndef Components_Lis2mdlDriver_HPP
#define Components_Lis2mdlDriver_HPP

#include "FprimeZephyrReference/Components/Drv/Lis2mdlDriver/Lis2mdlDriverComponentAc.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Drv {

class Lis2mdlDriver final : public Lis2mdlDriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Lis2mdlDriver object
    Lis2mdlDriver(const char* const compName);

    //! Destroy Lis2mdlDriver object
    ~Lis2mdlDriver();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Get the magnetic field reading from the LIS2MDL sensor
    Drv::MagneticField magneticFieldRead_handler(const FwIndexType portNum  //!< The port number
                                                 ) override;

    // ----------------------------------------------------------------------
    // Helper methods
    // ----------------------------------------------------------------------

    //! Convert a Zephyr sensor_value to an Fprime F64
    F64 sensor_value_to_f64(const struct sensor_value& val);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized LIS2MDL sensor
    const struct device* lis2mdl;
};

}  // namespace Drv

#endif
