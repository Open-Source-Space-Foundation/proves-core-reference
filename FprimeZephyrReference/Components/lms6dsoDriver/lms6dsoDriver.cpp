// ======================================================================
// \title  lms6dsoDriver.cpp
// \author aaron
// \brief  cpp file for lms6dsoDriver component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/lms6dsoDriver/lms6dsoDriver.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

lms6dsoDriver ::lms6dsoDriver(const char* const compName) : lms6dsoDriverComponentBase(compName) {
    // Initialize the LSM6DSO sensor
    lsm6dso = DEVICE_DT_GET_ONE(st_lsm6dso);
    FW_ASSERT(device_is_ready(lsm6dso));
    // Configure the LSM6DSO sensor
    struct sensor_value odr = {.val1 = 12, .val2 = 500000};  // 12.5 Hz
    sensor_attr_set(lsm6dso, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
    sensor_attr_set(lsm6dso, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
}

lms6dsoDriver ::~lms6dsoDriver() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Components::Acceleration lms6dsoDriver::getAcceleration_handler(FwIndexType portNum) {
    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;

    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_X, &x);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_Y, &y);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_Z, &z);

    return Components::Acceleration(this->sensor_value_to_f64(x),this->sensor_value_to_f64(y), this->sensor_value_to_f64(z));
}

Components::AngularVelocity lms6dsoDriver::getAngularVelocity_handler(FwIndexType portNum) {
    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;

    sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_X, &x);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_Y, &y);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_Z, &z);

    return Components::AngularVelocity(this->sensor_value_to_f64(x),this->sensor_value_to_f64(y), this->sensor_value_to_f64(z));
}


F64 lms6dsoDriver::getTemperature_handler(FwIndexType portNum) {
    struct sensor_value temp;

    sensor_channel_get(lsm6dso, SENSOR_CHAN_DIE_TEMP, &temp);

    return this->sensor_value_to_f64(temp);
}

F64 lms6dsoDriver::sensor_value_to_f64(const struct sensor_value& val) {
    return val.val1 + val.val2 / 1000000.0f;
}

}  // namespace Components
