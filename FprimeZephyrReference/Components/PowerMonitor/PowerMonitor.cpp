// ======================================================================
// \title  PowerMonitor.cpp
// \brief  cpp file for PowerMonitor component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/PowerMonitor/PowerMonitor.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PowerMonitor ::PowerMonitor(const char* const compName) : PowerMonitorComponentBase(compName) {}

PowerMonitor ::~PowerMonitor() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PowerMonitor ::run_handler(FwIndexType portNum, U32 context) {
    // System Power Monitor INA219 Requests
    this->sysVoltageGet_out(0);
    this->sysCurrentGet_out(0);
    this->sysPowerGet_out(0);

    // Solar Panel Power Monitor INA219 Requests
    this->solVoltageGet_out(0);
    this->solCurrentGet_out(0);
    this->solPowerGet_out(0);
}

}  // namespace Components
