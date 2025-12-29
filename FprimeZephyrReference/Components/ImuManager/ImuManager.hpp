// ======================================================================
// \title  ImuManager.hpp
// \brief  hpp file for ImuManager component implementation class
// ======================================================================

#ifndef Components_ImuManager_HPP
#define Components_ImuManager_HPP

#include "FprimeZephyrReference/Components/ImuManager/ImuManagerComponentAc.hpp"
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

namespace Components {

class ImuManager final : public ImuManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ImuManager object
    ImuManager(const char* const compName);

    //! Destroy ImuManager object
    ~ImuManager();

  public:
    // ----------------------------------------------------------------------
    // Helper methods
    // ----------------------------------------------------------------------

    //! Configure the IMU devices
    void configure(const struct device* lis2mdl, const struct device* lsm6dso);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for accelerationGet
    //!
    //! Port to read the current acceleration in m/s^2.
    Drv::Acceleration accelerationGet_handler(FwIndexType portNum,  //!< The port number
                                              Fw::Success& condition) override;

    //! Handler implementation for angularVelocityGet
    //!
    //! Port to read the current angular velocity in rad/s.
    Drv::AngularVelocity angularVelocityGet_handler(FwIndexType portNum,  //!< The port number
                                                    Fw::Success& condition) override;

    //! Handler implementation for magneticFieldGet
    //!
    //! Port to read the current magnetic field in gauss.
    Drv::MagneticField magneticFieldGet_handler(FwIndexType portNum,  //!< The port number
                                                Fw::Success& condition) override;

    //! Handler implementation for magneticFieldPeriodGet
    //!
    //! Port to get the time between magnetic field reads
    Fw::TimeIntervalValue magneticFieldSamplingPeriodGet_handler(FwIndexType portNum,  //!< The port number
                                                                 Fw::Success& condition) override;

    //! Handler implementation for run
    //!
    //! Port to trigger periodic data fetching and telemetry updating
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------
    //! Configure imu sensors
    void configureSensors(struct sensor_value& magn, struct sensor_value& accel, struct sensor_value& gyro);

    //! Apply axis orientation parameter to sensor readings
    void applyAxisOrientation(struct sensor_value& x_val, struct sensor_value& y_val, struct sensor_value& z_val);

    //! Get accelerometer sampling frequency from parameter
    struct sensor_value getAccelerometerSamplingFrequency();

    //! Get gyroscope sampling frequency from parameter
    struct sensor_value getGyroscopeSamplingFrequency();

    //! Get LSM6DSO sampling frequency from parameter
    struct sensor_value getLsm6dsoSamplingFrequency(Components::Lsm6dsoSamplingFrequency freqParam);

    //! Get magnetometer sampling frequency from parameter
    struct sensor_value getMagnetometerSamplingFrequency();

    bool sensorValuesEqual(struct sensor_value* sv1, struct sensor_value* sv2);

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    //! Zephyr device storing the initialized LIS2MDL sensor
    const struct device* m_lis2mdl;

    //! Zephyr device storing the initialized LSM6DSO sensor
    const struct device* m_lsm6dso;

    //! Current odr values for sensors
    struct sensor_value m_curr_magn_odr;
    struct sensor_value m_curr_gyro_odr;
    struct sensor_value m_curr_accel_odr;
};

}  // namespace Components

#endif
