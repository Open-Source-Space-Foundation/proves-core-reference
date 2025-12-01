// ======================================================================
// \title  ThermalManager.cpp
// \brief  cpp file for ThermalManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ThermalManager/ThermalManager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ThermalManager::ThermalManager(const char* const compName) : ThermalManagerComponentBase(compName) {}

ThermalManager::~ThermalManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ThermalManager::run_handler(FwIndexType portNum, U32 context) {
    // Cube face sensors (5 sensors)
    Fw::Success condition;  // We're not going to act on this condition for now

    this->face0TempGet_out(0, condition);
    this->face1TempGet_out(0, condition);
    this->face2TempGet_out(0, condition);
    this->face3TempGet_out(0, condition);
    this->face5TempGet_out(0, condition);

    // // If mux channel 4 is not healthy, skip battery cell sensors
    // if (this->muxChannel4HealthGet_out(0) != Fw::Health::HEALTHY) {
    //     return;
    // }

    // Battery cell sensors (4 sensors)
    this->battCell1TempGet_out(0, condition);
    this->battCell2TempGet_out(0, condition);
    this->battCell3TempGet_out(0, condition);
    this->battCell4TempGet_out(0, condition);
}

}  // namespace Components
