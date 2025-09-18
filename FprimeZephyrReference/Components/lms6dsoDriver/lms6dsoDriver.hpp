// ======================================================================
// \title  lms6dsoDriver.hpp
// \author aaron
// \brief  hpp file for lms6dsoDriver component implementation class
// ======================================================================

#ifndef Components_lms6dsoDriver_HPP
#define Components_lms6dsoDriver_HPP

#include "FprimeZephyrReference/Components/lms6dsoDriver/lms6dsoDriverComponentAc.hpp"


#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Components {

class lms6dsoDriver final : public lms6dsoDriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct lms6dsoDriver object
    lms6dsoDriver(const char* const compName  //!< The component name
    );

    //! Destroy lms6dsoDriver object
    ~lms6dsoDriver();

  private:
    
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------
    Acceleration getAcceleration_handler(FwIndexType portNum) override;

    AngularVelocity getAngularVelocity_handler(FwIndexType portNum) override;

    F64 getTemperature_handler(FwIndexType portNum) override;

    F64 sensor_value_to_f64(const struct sensor_value &val);

    //! Zephyr device stores the initialized LSM6DSO sensor
    const struct device* lsm6dso;


};

}  // namespace Components

#endif
