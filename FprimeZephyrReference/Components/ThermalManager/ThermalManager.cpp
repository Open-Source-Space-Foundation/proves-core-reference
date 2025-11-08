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
    // Read all 11 temperature sensors
    // Cube face sensors (6 sensors)
    this->face0TempGet_out(0);
    this->face1TempGet_out(0);
    this->face2TempGet_out(0);
    this->face3TempGet_out(0);
    this->face4TempGet_out(0);
    this->face5TempGet_out(0);

    // Top sensor
    this->topTempGet_out(0);

    // Battery cell sensors (4 sensors)
    this->battCell1TempGet_out(0);
    this->battCell2TempGet_out(0);
    this->battCell3TempGet_out(0);
    this->battCell4TempGet_out(0);
}

}  // namespace Components
