// ======================================================================
// \title  Lsm6dsoDriver.cpp
// \brief  cpp file for Lsm6dsoDriver component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Lsm6dsoDriver/Lsm6dsoDriver.hpp"

#include <Fw/Types/Assert.hpp>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Lsm6dsoDriver ::Lsm6dsoDriver(const char* const compName) : Lsm6dsoDriverComponentBase(compName) {
    // Initialize the LSM6DSO sensor
    lsm6dso = DEVICE_DT_GET_ONE(st_lsm6dso);
    FW_ASSERT(device_is_ready(lsm6dso));

    // Configure the LSM6DSO sensor
    struct sensor_value odr = {.val1 = 12, .val2 = 500000};  // 12.5 Hz
    FW_ASSERT(sensor_attr_set(lsm6dso, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr) == 0);
    FW_ASSERT(sensor_attr_set(lsm6dso, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr) == 0);
}

Lsm6dsoDriver ::~Lsm6dsoDriver() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Drv::Acceleration Lsm6dsoDriver ::accelerationRead_handler(FwIndexType portNum) {
    if (!device_is_ready(lsm6dso)) {
        this->log_WARNING_HI_Lsm6dsoNotReady();
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

Drv::AngularVelocity Lsm6dsoDriver ::angularVelocityRead_handler(FwIndexType portNum) {
    if (!device_is_ready(lsm6dso)) {
        this->log_WARNING_HI_Lsm6dsoNotReady();
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

F64 Lsm6dsoDriver ::temperatureRead_handler(FwIndexType portNum) {
    if (!device_is_ready(lsm6dso)) {
        this->log_WARNING_HI_Lsm6dsoNotReady();
        return 0;
    }

    struct sensor_value temp;

    sensor_sample_fetch_chan(lsm6dso, SENSOR_CHAN_DIE_TEMP);

    sensor_channel_get(lsm6dso, SENSOR_CHAN_DIE_TEMP, &temp);

    return Drv::sensor_value_to_f64(temp);
}

}  // namespace Drv
