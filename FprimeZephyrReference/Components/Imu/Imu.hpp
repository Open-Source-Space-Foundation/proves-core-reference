// ======================================================================
// \title  Imu.hpp
// \author aychar
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
    Imu(const char* const compName  //!< The component name
    );

    //! Destroy Imu object
    ~Imu();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for TODO
    //!
    //! TODO
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    F64 sensor_value_to_f64(const struct sensor_value& val);

    Components::Imu_Acceleration get_acceleration();
    Components::Imu_AngularVelocity get_angular_velocity();
    Components::Imu_MagneticField get_magnetic_field();

    const struct device* lis2mdl;
    const struct device* lsm6dso;
};

}  // namespace Components

#endif
