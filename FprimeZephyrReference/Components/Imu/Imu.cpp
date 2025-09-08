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

    lsm6dso = DEVICE_DT_GET_ONE(st_lsm6dso);
    FW_ASSERT(lsm6dso != nullptr);
    FW_ASSERT(device_is_ready(lsm6dso));

    odr = { .val1 = 12, .val2 = 500000 }; // 12.5 Hz
    sensor_attr_set(lsm6dso, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
    sensor_attr_set(lsm6dso, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);
}

Imu ::~Imu() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Imu ::run_handler(FwIndexType portNum, U32 context) {
    // Fetch new data sample
    struct sensor_value mag_field_data[3] = {};
    sensor_sample_fetch_chan(lis2mdl, SENSOR_CHAN_MAGN_XYZ);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_XYZ, mag_field_data);
    float magnt_values[3] = {out_ev(&mag_field_data[0]), out_ev(&mag_field_data[1]), out_ev(&mag_field_data[2])};

    struct sensor_value accel_data[3] = {};
    sensor_sample_fetch_chan(lsm6dso, SENSOR_CHAN_ACCEL_XYZ);
    sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_XYZ, accel_data);
    float accel_values[3] = {out_ev(&accel_data[0]), out_ev(&accel_data[1]), out_ev(&accel_data[2])};

    // struct sensor_value gyro_data[3] = {};
    // sensor_sample_fetch_chan(lsm6dso, SENSOR_CHAN_GYRO_XYZ);
    // sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_XYZ, gyro_data);
    // float gyro_values[3] = {out_ev(&gyro_data[0]), out_ev(&gyro_data[1]), out_ev(&gyro_data[2])};


    // Output the magnetic field values via telemetry
    this->tlmWrite_MagneticField(Components::Imu_MagneticField(
        static_cast<F64>(magnt_values[0]), static_cast<F64>(magnt_values[1]), static_cast<F64>(magnt_values[2])));

    // Output the acceleration values via telemtry
    this->tlmWrite_Acceleration(Components::Imu_Acceleration(
        static_cast<F64>(accel_values[0]), static_cast<F64>(accel_values[1]), static_cast<F64>(accel_values[2])));

    // //Output the gyroscope values via telemtry
    // this->tlmWrite_Gyroscope(Components::Imu_Gyroscope(
    //     static_cast<F64>(gyro_values[0]), static_cast<F64>(gyro_values[1]), static_cast<F64>(gyro_values[2])));
    
}

float Imu::out_ev(struct sensor_value *val) {
    return val->val1 + (float)val->val2 / 1000000.0f;
}


}  // namespace Components
