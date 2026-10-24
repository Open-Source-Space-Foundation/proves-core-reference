// ======================================================================
// \title  PowerMonitor.cpp
// \brief  cpp file for PowerMonitor component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/PowerMonitor/PowerMonitor.hpp"

#include <Fw/Time/Time.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PowerMonitor ::PowerMonitor(const char* const compName) : PowerMonitorComponentBase(compName), m_integrator() {}

PowerMonitor ::~PowerMonitor() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PowerMonitor ::run_handler(FwIndexType portNum, U32 context) {
    // System Power Monitor Requests
    this->sysVoltageGet_out(0);
    this->sysCurrentGet_out(0);
    F64 sysPowerW = this->sysPowerGet_out(0);

    // Solar Panel Power Monitor Requests
    this->solVoltageGet_out(0);
    this->solCurrentGet_out(0);
    F64 solPowerW = this->solPowerGet_out(0);

    // Update total power consumption with combined system and solar power, and total solar power generation.
    // Both totals are integrated over the same time step so each is credited with the full tick period.
    F64 totalPowerW = sysPowerW + solPowerW;
    this->m_integrator.update(this->getCurrentTimeSeconds(), totalPowerW, solPowerW);

    // Emit telemetry updates
    this->tlmWrite_TotalPowerConsumption(this->m_integrator.getConsumption_mWh());
    this->tlmWrite_TotalPowerGenerated(this->m_integrator.getGeneration_mWh());
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PowerMonitor ::RESET_TOTAL_POWER_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->m_integrator.resetConsumption();
    this->log_ACTIVITY_LO_TotalPowerReset();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void PowerMonitor ::RESET_TOTAL_GENERATION_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->m_integrator.resetGeneration();
    this->log_ACTIVITY_LO_TotalGenerationReset();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void PowerMonitor ::GET_TOTAL_POWER_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->log_ACTIVITY_LO_TotalPowerConsumptionReading(this->m_integrator.getConsumption_mWh());
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Helper method implementations
// ----------------------------------------------------------------------

F64 PowerMonitor ::getCurrentTimeSeconds() {
    Fw::Time t = this->getTime();
    return static_cast<F64>(t.getSeconds()) + (static_cast<F64>(t.getUSeconds()) / 1.0e6);
}

}  // namespace Components
