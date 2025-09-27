// ======================================================================
// \title  Lis2mdlManager.cpp
// \brief  cpp file for Lis2mdlManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Lis2mdlManager/Lis2mdlManager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Lis2mdlManager ::Lis2mdlManager(const char* const compName) : Lis2mdlManagerComponentBase(compName) {
    // Initialize the lis2mdl sensor
    lis2mdl = device_get_binding("LIS2MDL");
}

Lis2mdlManager ::~Lis2mdlManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Drv::MagneticField Lis2mdlManager ::magneticFieldRead_handler(FwIndexType portNum) {
    if (!device_is_ready(lis2mdl)) {
        this->log_WARNING_HI_DeviceNotReady();
        return Drv::MagneticField(0.0, 0.0, 0.0);
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value x;
    struct sensor_value y;
    struct sensor_value z;

    sensor_sample_fetch_chan(lis2mdl, SENSOR_CHAN_MAGN_XYZ);

    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_X, &x);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Y, &y);
    sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_Z, &z);

    Drv::MagneticField magnetic_readings =
        Drv::MagneticField(Drv::sensor_value_to_f64(x), Drv::sensor_value_to_f64(y), Drv::sensor_value_to_f64(z));

    this->tlmWrite_MagneticField(magnetic_readings);

    return magnetic_readings;
}

}  // namespace Drv
