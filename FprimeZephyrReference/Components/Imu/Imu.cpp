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
    this->imuCallCount++;
    this->tlmWrite_ImuCalls(this->imuCallCount);
    this->log_ACTIVITY_HI_ImuTestEvent();

    sensor_sample_fetch_chan(lis2mdl, SENSOR_CHAN_MAGN_XYZ);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_X, &magnetic_field_x);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Y, &magnetic_field_y);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Z, &magnetic_field_z);
}

}  // namespace Components
