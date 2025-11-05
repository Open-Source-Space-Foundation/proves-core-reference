// ======================================================================
// \title  PowerMonitor.cpp
// \brief  cpp file for PowerMonitor component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/PowerMonitor/PowerMonitor.hpp"
#include <Fw/Time/Time.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PowerMonitor ::PowerMonitor(const char* const compName)
    : PowerMonitorComponentBase(compName), m_totalPower_mWh(0.0f), m_lastUpdateTime_s(0.0) {}

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

    // Update total power consumption with combined system and solar power
    F64 totalPowerW = sysPowerW + solPowerW;
    this->updatePower(totalPowerW);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PowerMonitor ::RESET_TOTAL_POWER_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->m_totalPower_mWh = 0.0f;
    this->m_lastUpdateTime_s = this->getCurrentTimeSeconds();
    this->log_ACTIVITY_LO_TotalPowerReset();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Helper method implementations
// ----------------------------------------------------------------------

F64 PowerMonitor ::getCurrentTimeSeconds() {
    Fw::Time t = this->getTime();
    return static_cast<F64>(t.getSeconds()) + (static_cast<F64>(t.getUSeconds()) / 1.0e6);
}

void PowerMonitor ::updatePower(F64 powerW) {
    F64 now_s = this->getCurrentTimeSeconds();

    // Initialize time on first call
    if (this->m_lastUpdateTime_s == 0.0) {
        this->m_lastUpdateTime_s = now_s;
        return;
    }

    F64 dt_s = now_s - this->m_lastUpdateTime_s;

    // Only accumulate if time has passed and delta is reasonable (< 10 seconds to avoid time jumps)
    if (dt_s > 0.0 && dt_s < 10.0) {
        // Convert to mWh: Power (W) * time (hours) * 1000
        F32 energyAdded_mWh = static_cast<F32>(powerW * (dt_s / 3600.0) * 1000.0);
        this->m_totalPower_mWh += energyAdded_mWh;
    }

    this->m_lastUpdateTime_s = now_s;

    // Emit telemetry update
    this->tlmWrite_TotalPowerConsumption(this->m_totalPower_mWh);
}

}  // namespace Components
