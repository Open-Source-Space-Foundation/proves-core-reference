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
    // Initialize the LSM6DSO sensor
    lsm6dso = DEVICE_DT_GET_ONE(st_lsm6dso);

    // Configure the LSM6DSO sensor
    struct sensor_value odr = {.val1 = 12, .val2 = 500000};  // 12.5 Hz
}

Lsm6dsoManager ::~Lsm6dsoManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Drv::Acceleration Lsm6dsoManager ::accelerationRead_handler(FwIndexType portNum) {
    if (!device_is_ready(lsm6dso)) {
        this->log_WARNING_HI_DeviceNotReady();
        return Drv::Acceleration(0.0, 0.0, 0.0);
    }

    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;

    sensor_sample_fetch_chan(lsm6dso, SENSOR_CHAN_ACCEL_XYZ);

    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_X, &x);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_Y, &y);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_Z, &z);

    return Drv::Acceleration(Drv::sensor_value_to_f64(x), Drv::sensor_value_to_f64(y), Drv::sensor_value_to_f64(z));
}

Drv::AngularVelocity Lsm6dsoManager ::angularVelocityRead_handler(FwIndexType portNum) {
    if (!device_is_ready(lsm6dso)) {
        this->log_WARNING_HI_DeviceNotReady();
        return Drv::AngularVelocity(0.0, 0.0, 0.0);
    }

    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;

    sensor_sample_fetch_chan(lsm6dso, SENSOR_CHAN_GYRO_XYZ);

    sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_X, &x);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_Y, &y);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_Z, &z);

    return Drv::AngularVelocity(Drv::sensor_value_to_f64(x), Drv::sensor_value_to_f64(y), Drv::sensor_value_to_f64(z));
}

F64 Lsm6dsoManager ::temperatureRead_handler(FwIndexType portNum) {
    if (!device_is_ready(lsm6dso)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }

    struct sensor_value temp;

    sensor_sample_fetch_chan(lsm6dso, SENSOR_CHAN_DIE_TEMP);

    sensor_channel_get(lsm6dso, SENSOR_CHAN_DIE_TEMP, &temp);

    return Drv::sensor_value_to_f64(temp);
}

}  // namespace Drv
