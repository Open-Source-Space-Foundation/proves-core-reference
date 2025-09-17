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

  
    //! Get the magnetic field reading from the IMU
    Components::Imu_MagneticField get_magnetic_field();


    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Zephyr device stores the initialized LIS2MDL sensor
    const struct device* lis2mdl;


};

}  // namespace Components

#endif
