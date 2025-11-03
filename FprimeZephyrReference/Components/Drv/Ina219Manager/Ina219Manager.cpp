// ======================================================================
// \title  Ina219Manager.cpp
// \author nate
// \brief  cpp file for Ina219Manager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Drv/Ina219Manager/Ina219Manager.hpp"
#include <Fw/Types/Assert.hpp>


namespace Drv {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Ina219Manager ::Ina219Manager(const char* const compName) : Ina219ManagerComponentBase(compName) {
    dev = device_get_binding("INA219");
}

Ina219Manager ::~Ina219Manager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

F64 Ina219Manager ::currentGet_handler(FwIndexType portNum) {
    if(!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value current; 

    sensor_sample_fetch_chan(dev, SENSOR_CHAN_CURRENT);

    sensor_channel_get(dev, SENSOR_CHAN_CURRENT, &current);

    this->tlmWrite_Current(sensor_value_to_double(&current));

    return sensor_value_to_double(&current);
}

F64 Ina219Manager ::powerGet_handler(FwIndexType portNum) {
    if(!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value power; 

    sensor_sample_fetch_chan(dev, SENSOR_CHAN_POWER);

    sensor_channel_get(dev, SENSOR_CHAN_POWER, &power);

    this->tlmWrite_Current(sensor_value_to_double(&power));
    
    return sensor_value_to_double(&power);
}

F64 Ina219Manager ::voltageGet_handler(FwIndexType portNum) {
    if(!device_is_ready(dev)) {
        this->log_WARNING_HI_DeviceNotReady();
        return 0;
    }
    this->log_WARNING_HI_DeviceNotReady_ThrottleClear();

    struct sensor_value voltage; 

    sensor_sample_fetch_chan(dev, SENSOR_CHAN_VOLTAGE);

    sensor_channel_get(dev, SENSOR_CHAN_VOLTAGE, &voltage);

    this->tlmWrite_Current(sensor_value_to_double(&voltage));
    
    return sensor_value_to_double(&voltage);
}

}  // namespace Drv
