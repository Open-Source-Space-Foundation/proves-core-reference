// ======================================================================
// \title  GenericDeviceMonitor.cpp
// \author nate
// \brief  cpp file for GenericDeviceMonitor component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/GenericDeviceMonitor/GenericDeviceMonitor.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

GenericDeviceMonitor ::GenericDeviceMonitor(const char* const compName) : GenericDeviceMonitorComponentBase(compName) {}

GenericDeviceMonitor ::~GenericDeviceMonitor() {}

// ----------------------------------------------------------------------
// Helper methods
// ----------------------------------------------------------------------

void GenericDeviceMonitor::configure(const struct device* dev) {
    this->m_dev = dev;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Fw::Health GenericDeviceMonitor ::healthGet_handler(FwIndexType portNum) {
    return device_is_ready(this->m_dev) ? Fw::Health::HEALTHY : Fw::Health::FAILED;
}

void GenericDeviceMonitor ::run_handler(FwIndexType portNum, U32 context) {
    if (!device_is_ready(this->m_dev)) {
        this->tlmWrite_Healthy(Fw::Health::FAILED);
        this->log_WARNING_LO_DeviceNotReady();
        return;
    }

    this->tlmWrite_Healthy(Fw::Health::HEALTHY);
}

}  // namespace Components
