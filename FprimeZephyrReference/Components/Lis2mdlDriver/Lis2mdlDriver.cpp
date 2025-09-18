// ======================================================================
// \title  Lis2mdlDriver.cpp
// \author aychar
// \brief  cpp file for Lis2mdlDriver component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Lis2mdlDriver/Lis2mdlDriver.hpp"

#include <Fw/Types/Assert.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Lis2mdlDriver ::Lis2mdlDriver(const char* const compName) : Lis2mdlDriverComponentBase(compName) {
    lis2mdl = device_get_binding("LIS2MDL");
    FW_ASSERT(device_is_ready(lis2mdl));
}

Lis2mdlDriver ::~Lis2mdlDriver() {}

F64 Lis2mdlDriver ::sensor_value_to_f64(const struct sensor_value& val) {
    return val.val1 + val.val2 / 1000000.0f;
}

Components::MagneticField Lis2mdlDriver ::getMagneticField_handler(FwIndexType portNum) {
    // Fetch new data sample from sensors
    sensor_sample_fetch_chan(lis2mdl, SENSOR_CHAN_MAGN_XYZ);

    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;

    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_X, &x);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Y, &y);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Z, &z);

    return Components::MagneticField(this->sensor_value_to_f64(x), this->sensor_value_to_f64(y),
                                     this->sensor_value_to_f64(z));
}

}  // namespace Components
