// ======================================================================
// \title  Lsm6dsoManager.cpp
// \brief  cpp file for Lsm6dsoManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Lsm6dsoManager/Lsm6dsoManager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Lsm6dsoManager ::Lsm6dsoManager(const char* const compName) : Lsm6dsoManagerComponentBase(compName) {
    dev = DEVICE_DT_GET_ONE(st_lsm6dso);

    // Configure the sensor
    struct sensor_value odr = {.val1 = 12, .val2 = 500000};  // 12.5 Hz

    if (sensor_attr_set(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr) != 0) {
        this->log_WARNING_HI_AccelerometerSamplingFrequencyNotConfigured();
    }

    if (sensor_attr_set(dev, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr) != 0) {
        this->log_WARNING_HI_GyroscopeSamplingFrequencyNotConfigured();
    }
}

Lsm6dsoManager ::~Lsm6dsoManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Drv::Acceleration Lsm6dsoManager ::accelerationGet_handler(FwIndexType portNum) {
    if (!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return Drv::Acceleration(0.0, 0.0, 0.0);
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;

    sensor_sample_fetch_chan(dev, SENSOR_CHAN_ACCEL_XYZ);

    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &x);
    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Y, &y);
    sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Z, &z);

    Drv::Acceleration acceleration =
        Drv::Acceleration(Drv::sensor_value_to_f64(x), Drv::sensor_value_to_f64(y), Drv::sensor_value_to_f64(z));

    this->tlmWrite_Acceleration(acceleration);
    return acceleration;
}

Drv::AngularVelocity Lsm6dsoManager ::angularVelocityGet_handler(FwIndexType portNum) {
    if (!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return Drv::AngularVelocity(0.0, 0.0, 0.0);
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;

    sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);

    sensor_channel_get(dev, SENSOR_CHAN_GYRO_X, &x);
    sensor_channel_get(dev, SENSOR_CHAN_GYRO_Y, &y);
    sensor_channel_get(dev, SENSOR_CHAN_GYRO_Z, &z);

    Drv::AngularVelocity angular_velocity =
        Drv::AngularVelocity(Drv::sensor_value_to_f64(x), Drv::sensor_value_to_f64(y), Drv::sensor_value_to_f64(z));

    this->tlmWrite_AngularVelocity(angular_velocity);
    return angular_velocity;
}

F64 Lsm6dsoManager ::temperatureGet_handler(FwIndexType portNum) {
    if (!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value temp;

    sensor_sample_fetch_chan(dev, SENSOR_CHAN_DIE_TEMP);

    sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &temp);

    this->tlmWrite_Temperature(Drv::sensor_value_to_f64(temp));

    return Drv::sensor_value_to_f64(temp);
}

}  // namespace Drv
