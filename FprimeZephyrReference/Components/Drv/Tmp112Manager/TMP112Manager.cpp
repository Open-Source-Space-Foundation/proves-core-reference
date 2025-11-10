// ======================================================================
// \title  TMP112Manager.cpp
// \brief  cpp file for TMP112Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Tmp112Manager/TMP112Manager.hpp"
#include <Fw/Types/Assert.hpp>

namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TMP112Manager ::TMP112Manager(const char* const compName) : TMP112ManagerComponentBase(compName) {}

TMP112Manager ::~TMP112Manager() {}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void TMP112Manager::configure(const struct device* dev) {
    this->m_dev = dev;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

F64 TMP112Manager ::ambientTemperatureGet_handler(FwIndexType portNum) {
    if (!device_is_ready(this->m_dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value temp;

    int rc = sensor_sample_fetch_chan(this->m_dev, SENSOR_CHAN_AMBIENT_TEMP);
    if (rc != 0) {
        // Sensor fetch failed - return 0 to indicate error
        return 0;
    }

    rc = sensor_channel_get(this->m_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
    if (rc != 0) {
        // Channel get failed - return 0 to indicate error
        return 0;
    }

    this->tlmWrite_AmbientTemperature(sensor_value_to_double(&temp));

    return sensor_value_to_double(&temp);
}

}  // namespace Drv
