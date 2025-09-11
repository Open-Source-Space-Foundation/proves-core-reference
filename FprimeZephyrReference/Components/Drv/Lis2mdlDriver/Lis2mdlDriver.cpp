// ======================================================================
// \title  Lis2mdlDriver.cpp
// \brief  cpp file for Lis2mdlDriver component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Lis2mdlDriver/Lis2mdlDriver.hpp"

#include <Fw/Types/Assert.hpp>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Lis2mdlDriver ::Lis2mdlDriver(const char* const compName) : Lis2mdlDriverComponentBase(compName) {
    // Initialize the lis2mdl sensor
    lis2mdl = device_get_binding("LIS2MDL");
    FW_ASSERT(device_is_ready(lis2mdl));
}

Lis2mdlDriver ::~Lis2mdlDriver() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Drv::MagneticField Lis2mdlDriver ::magneticFieldRead_handler(FwIndexType portNum) {
    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;

    sensor_sample_fetch_chan(lis2mdl, SENSOR_CHAN_MAGN_XYZ);

    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_X, &x);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Y, &y);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Z, &z);

    return Drv::MagneticField(this->sensor_value_to_f64(x), this->sensor_value_to_f64(y), this->sensor_value_to_f64(z));
}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

F64 Lis2mdlDriver ::sensor_value_to_f64(const struct sensor_value& val) {
    return val.val1 + val.val2 / 1000000.0f;
}

}  // namespace Drv
