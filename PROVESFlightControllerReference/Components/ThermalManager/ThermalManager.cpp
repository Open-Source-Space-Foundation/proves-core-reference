// ======================================================================
// \title  ThermalManager.cpp
// \brief  cpp file for ThermalManager component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/ThermalManager/ThermalManager.hpp"

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
    Fw::Success condition;
    Fw::ParamValid param_valid;

    // Face temp sensors
    for (FwIndexType i = 0; i < this->getNum_faceTempGet_OutputPorts(); i++) {
        F64 temperature = this->faceTempGet_out(i, condition);
        if (temperature < this->paramGet_FACE_TEMP_LOWER_THRESHOLD(param_valid)) {
            this->log_WARNING_LO_FaceTemperatureBelowThreshold(i, temperature);
        } else if (temperature > this->paramGet_FACE_TEMP_UPPER_THRESHOLD(param_valid)) {
            this->log_WARNING_LO_FaceTemperatureAboveThreshold(i, temperature);
        }
    }

    // Battery cell temp sensors
    for (FwIndexType i = 0; i < this->getNum_battCellTempGet_OutputPorts(); i++) {
        F64 temperature = this->battCellTempGet_out(i, condition);
        if (temperature < this->paramGet_BATT_CELL_TEMP_LOWER_THRESHOLD(param_valid)) {
            this->log_WARNING_LO_BatteryCellTemperatureBelowThreshold(i, temperature);
        } else if (temperature > this->paramGet_BATT_CELL_TEMP_UPPER_THRESHOLD(param_valid)) {
            this->log_WARNING_LO_BatteryCellTemperatureAboveThreshold(i, temperature);
        }
    }
}

}  // namespace Components
