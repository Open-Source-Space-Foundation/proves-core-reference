// ======================================================================
// \title  Imu.hpp
// \brief  hpp file for Imu component implementation class
// ======================================================================

#ifndef Components_Imu_HPP
#define Components_Imu_HPP

#include "FprimeZephyrReference/Components/Imu/ImuComponentAc.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

namespace Components {

class Imu final : public ImuComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Imu object
    Imu(const char* const compName);

    //! Destroy Imu object
    ~Imu();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    // ----------------------------------------------------------------------
    // Helper methods
    // ----------------------------------------------------------------------

    //! Convert a Zephyr sensor_value to an Fprime F64
    F64 sensor_value_to_f64(const struct sensor_value& val);

    // ----------------------------------------------------------------------
    // IMU access methods
    // ----------------------------------------------------------------------

    //! Get the acceleration reading from the IMU
    Components::Imu_Acceleration get_acceleration();

    //! Get the angular velocity reading from the IMU
    Components::Imu_AngularVelocity get_angular_velocity();

    //! Get the temperature reading from the IMU
    F64 get_temperature();

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized LSM6DSO sensor
    const struct device* lsm6dso;
};

}  // namespace Components

#endif
