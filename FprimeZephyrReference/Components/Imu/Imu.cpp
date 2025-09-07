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
    lis2mdl = device_get_binding("LIS2MDL");
    FW_ASSERT(lis2mdl != nullptr);
    FW_ASSERT(device_is_ready(lis2mdl));

    lsm6dso = device_get_binding("LSM6DSO");
    FW_ASSERT(lsm6dso != nullptr);
    FW_ASSERT(device_is_ready(lsm6dso));
}

Imu ::~Imu() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Imu ::run_handler(FwIndexType portNum, U32 context) {
    // Fetch new data sample
    sensor_sample_fetch_chan(lis2mdl, SENSOR_CHAN_MAGN_XYZ);

    // Extract the data from the sample
    struct sensor_value magnetic_data_x;
    struct sensor_value magnetic_data_y;
    struct sensor_value magnetic_data_z;
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_X, &magnetic_data_x);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Y, &magnetic_data_y);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Z, &magnetic_data_z);

    // Convert to float values in gauss
    float magnetic_field_x = out_ev(&magnetic_data_x);
    float magnetic_field_y = out_ev(&magnetic_data_y);
    float magnetic_field_z = out_ev(&magnetic_data_z);

    struct sensor_value accel_data[3] = {};
    sensor_sample_fetch_chan(lsm6dso, SENSOR_CHAN_ACCEL_XYZ);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_XYZ, accel_data);
    float accel_values[3] = {out_ev(&accel_data[0]), out_ev(&accel_data[1]), out_ev(&accel_data[2])};

    struct sensor_value gyro_data[3] = {};
    sensor_sample_fetch_chan(lsm6dso, SENSOR_CHAN_GYRO_XYZ);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_XYZ, gyro_data);
    float gyro_values[3] = {out_ev(gyro_data[0]), out_ev(gyro_data[1]), out_ev(gyro_data[2])};


    // Output the magnetic field values via telemetry
    this->tlmWrite_MagneticField(Components::Imu_MagneticField(
        static_cast<F64>(magnetic_field_x), static_cast<F64>(magnetic_field_y), static_cast<F64>(magnetic_field_z)));

    // Output the acceleration values vie telemtry
    this->tlmWrite_Acceleration(Components::Imu_Acceleration(
        static_cast<F64>(accel_values[0]), static_cast<F64>(accel_values[1]), static_cast<F64>(accel_values[2])));

    this->tlmWrite_Gyroscope(Components::Imu_Gyroscope(
        static_cast<F64>(gyro_values[0]), static_cast<F64>(gyro_values[1]), static_cast<F64>(gyro_values[2])));
    
}

float Imu::out_ev(struct sensor_value *val) {
    return val->val1 + (float)val->val2 / 1000000.0f;
}


}  // namespace Components
