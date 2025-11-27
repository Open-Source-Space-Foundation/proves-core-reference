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
    // Read all 11 temperature sensors and write telemetry

    // Cube face sensors (6 sensors)
    F64 face0Temp = this->face0TempGet_out(0);
    this->tlmWrite_Face0Temperature(face0Temp);

    F64 face1Temp = this->face1TempGet_out(0);
    this->tlmWrite_Face1Temperature(face1Temp);

    F64 face2Temp = this->face2TempGet_out(0);
    this->tlmWrite_Face2Temperature(face2Temp);

    F64 face3Temp = this->face3TempGet_out(0);
    this->tlmWrite_Face3Temperature(face3Temp);

    F64 face4Temp = this->face4TempGet_out(0);
    this->tlmWrite_Face4Temperature(face4Temp);

    F64 face5Temp = this->face5TempGet_out(0);
    this->tlmWrite_Face5Temperature(face5Temp);

    // Top sensor
    F64 topTemp = this->topTempGet_out(0);
    this->tlmWrite_TopTemperature(topTemp);

    // Battery cell sensors (4 sensors)
    F64 battCell1Temp = this->battCell1TempGet_out(0);
    this->tlmWrite_BattCell1Temperature(battCell1Temp);

    F64 battCell2Temp = this->battCell2TempGet_out(0);
    this->tlmWrite_BattCell2Temperature(battCell2Temp);

    F64 battCell3Temp = this->battCell3TempGet_out(0);
    this->tlmWrite_BattCell3Temperature(battCell3Temp);

    F64 battCell4Temp = this->battCell4TempGet_out(0);
    this->tlmWrite_BattCell4Temperature(battCell4Temp);
}

}  // namespace Components
