// ======================================================================
// \title  Imu.cpp
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

}

Imu ::~Imu() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Imu ::run_handler(FwIndexType portNum, U32 context) {
    // Fetch new data samples from the sensors
    sensor_sample_fetch_chan(lis2mdl, SENSOR_CHAN_MAGN_XYZ);


    // Output sensor values via telemetry
    this->tlmWrite_Acceleration(this->readAcceleration_out(0));
    this->tlmWrite_AngularVelocity(this->readAngularVelocity_out(0));
    this->tlmWrite_Temperature(this->readTemperature_out(0));

    this->tlmWrite_MagneticField(this->get_magnetic_field());

}

F64 Imu ::sensor_value_to_f64(const struct sensor_value& val) {
    return val.val1 + val.val2 / 1000000.0f;
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
