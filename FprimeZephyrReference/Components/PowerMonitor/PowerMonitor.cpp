// ======================================================================
// \title  PowerMonitor.cpp
// \author nate
// \brief  cpp file for PowerMonitor component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/PowerMonitor/PowerMonitor.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PowerMonitor ::PowerMonitor(const char* const compName) : PowerMonitorComponentBase(compName) {}

PowerMonitor ::~PowerMonitor() {}

void PowerMonitor ::run_handler(FwIndexType portNum, U32 context) {
    this->voltageGet_out(0);
    this->currentGet_out(0);
    this->powerGet_out(0);
}

}  // namespace Components
