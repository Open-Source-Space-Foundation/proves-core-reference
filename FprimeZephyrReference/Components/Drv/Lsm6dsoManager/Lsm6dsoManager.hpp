// ======================================================================
// \title  Lsm6dsoManager.hpp
// \brief  hpp file for Lsm6dsoManager component implementation class
// ======================================================================

#ifndef Components_Lsm6dsoManager_HPP
#define Components_Lsm6dsoManager_HPP

// clang-format off
// Keep the includes in this order
#include "FprimeZephyrReference/Components/Drv/Lsm6dsoManager/Lsm6dsoManagerComponentAc.hpp"
#include "FprimeZephyrReference/Components/Drv/Helpers/Helpers.hpp"
// clang-format on

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Drv {

class Lsm6dsoManager final : public Lsm6dsoManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Lsm6dsoManager object
    Lsm6dsoManager(const char* const compName);

    //! Destroy Lsm6dsoManager object
    ~Lsm6dsoManager();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Get the acceleration reading from the LSM6DSO sensor
    Drv::Acceleration accelerationGet_handler(const FwIndexType portNum  //!< The port number
                                              ) override;

    //! Get the angular velocity reading from the LSM6DSO sensor
    Drv::AngularVelocity angularVelocityGet_handler(const FwIndexType portNum  //!< The port number
                                                    ) override;

    //! Get the temperature reading from the LSM6DSO sensor
    F64 temperatureGet_handler(const FwIndexType portNum  //!< The port number
                               ) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized LSM6DSO sensor
    const struct device* dev;
};

}  // namespace Drv

#endif
