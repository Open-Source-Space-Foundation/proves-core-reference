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
    float magnetic_field_x = magnetic_data_x.val1 + magnetic_data_x.val2 / 1000000.0f;
    float magnetic_field_y = magnetic_data_y.val1 + magnetic_data_y.val2 / 1000000.0f;
    float magnetic_field_z = magnetic_data_z.val1 + magnetic_data_z.val2 / 1000000.0f;

    // Output the magnetic field values via telemetry
    this->tlmWrite_MagneticField(Components::Imu_MagneticField(
        static_cast<F64>(magnetic_field_x), static_cast<F64>(magnetic_field_y), static_cast<F64>(magnetic_field_z)));
}

}  // namespace Components
