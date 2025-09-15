// ======================================================================
// \title  Lis2mdlDriver.hpp
// \author aychar
// \brief  hpp file for Lis2mdlDriver component implementation class
// ======================================================================

#ifndef Components_Lis2mdlDriver_HPP
#define Components_Lis2mdlDriver_HPP

#include "FprimeZephyrReference/Components/Lis2mdlDriver/Lis2mdlDriverComponentAc.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Components {

class Lis2mdlDriver final : public Lis2mdlDriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Lis2mdlDriver object
    Lis2mdlDriver(const char* const compName  //!< The component name
    );

    //! Destroy Lis2mdlDriver object
    ~Lis2mdlDriver();

  private:
    //! Handler implementation
    MagneticField getMagneticField_handler(FwIndexType portNum) override;

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

}  // namespace Components

#endif
