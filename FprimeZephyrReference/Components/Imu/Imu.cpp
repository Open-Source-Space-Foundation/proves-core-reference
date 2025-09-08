// ======================================================================
// \title  Imu.cpp
// \author aychar
// \brief  cpp file for Imu component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Imu/Imu.hpp"

#include <Fw/Types/Assert.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Imu ::Imu(const char* const compName) : ImuComponentBase(compName) {
    // Initialize the LIS2MDL sensor
    lis2mdl = device_get_binding("LIS2MDL");
    FW_ASSERT(device_is_ready(lis2mdl));

    // Initialize the LSM6DSO sensor
    lsm6dso = DEVICE_DT_GET_ONE(st_lsm6dso);
    FW_ASSERT(device_is_ready(lsm6dso));

    // Configure the LSM6DSO sensor
    struct sensor_value odr = {.val1 = 12, .val2 = 500000};  // 12.5 Hz
    FW_ASSERT(sensor_attr_set(lsm6dso, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr) == 0);
    FW_ASSERT(sensor_attr_set(lsm6dso, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr) == 0);
}

Imu ::~Imu() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Imu ::run_handler(FwIndexType portNum, U32 context) {
    // Fetch new data sample
    sensor_sample_fetch_chan(lsm6dso, SENSOR_CHAN_ACCEL_XYZ);
    sensor_sample_fetch_chan(lsm6dso, SENSOR_CHAN_GYRO_XYZ);
    sensor_sample_fetch_chan(lis2mdl, SENSOR_CHAN_MAGN_XYZ);

    // Output the magnetic field values via telemetry
    this->tlmWrite_Acceleration(this->get_acceleration());
    this->tlmWrite_AngularVelocity(this->get_angular_velocity());
    this->tlmWrite_MagneticField(this->get_magnetic_field());
}

F64 Imu ::sensor_value_to_f64(const struct sensor_value& val) {
    return val.val1 + val.val2 / 1000000.0f;
}

Components::Imu_Acceleration Imu ::get_acceleration() {
    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;
    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_X, &x);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_Y, &y);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_Z, &z);

    return Components::Imu_Acceleration(this->sensor_value_to_f64(x), this->sensor_value_to_f64(y),
                                        this->sensor_value_to_f64(z));
}

Components::Imu_AngularVelocity Imu ::get_angular_velocity() {
    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;
    sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_X, &x);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_Y, &y);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_Z, &z);

    return Components::Imu_AngularVelocity(this->sensor_value_to_f64(x), this->sensor_value_to_f64(y),
                                           this->sensor_value_to_f64(z));
}

Components::Imu_MagneticField Imu ::get_magnetic_field() {
    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_X, &x);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Y, &y);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Z, &z);

    return Components::Imu_MagneticField(this->sensor_value_to_f64(x), this->sensor_value_to_f64(y),
                                         this->sensor_value_to_f64(z));
}

}  // namespace Components
