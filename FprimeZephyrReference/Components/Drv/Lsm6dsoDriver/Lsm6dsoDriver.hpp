// ======================================================================
// \title  Lsm6dsoDriver.hpp
// \brief  hpp file for Lsm6dsoDriver component implementation class
// ======================================================================

#ifndef Components_Lsm6dsoDriver_HPP
#define Components_Lsm6dsoDriver_HPP

// clang-format off
// Keep the includes in this order
#include "FprimeZephyrReference/Components/Drv/Lsm6dsoDriver/Lsm6dsoDriverComponentAc.hpp"
#include "FprimeZephyrReference/Components/Drv/Helpers/Helpers.hpp"
// clang-format on

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Drv {

class Lsm6dsoDriver final : public Lsm6dsoDriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Lsm6dsoDriver object
    Lsm6dsoDriver(const char* const compName);

    //! Destroy Lsm6dsoDriver object
    ~Lsm6dsoDriver();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Get the acceleration reading from the LSM6DSO sensor
    Drv::Acceleration accelerationRead_handler(const FwIndexType portNum  //!< The port number
                                               ) override;

    //! Get the angular velocity reading from the LSM6DSO sensor
    Drv::AngularVelocity angularVelocityRead_handler(const FwIndexType portNum  //!< The port number
                                                     ) override;

    //! Get the temperature reading from the LSM6DSO sensor
    F64 temperatureRead_handler(const FwIndexType portNum  //!< The port number
                                ) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized LSM6DSO sensor
    const struct device* lsm6dso;
};

}  // namespace Drv

#endif
