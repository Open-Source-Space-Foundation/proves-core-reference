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

    const F64 FACE_TEMP_LOWER_THRESHOLD = this->paramGet_FACE_TEMP_LOWER_THRESHOLD(param_valid);
    const F64 FACE_TEMP_UPPER_THRESHOLD = this->paramGet_FACE_TEMP_UPPER_THRESHOLD(param_valid);
    const F64 BATT_CELL_TEMP_LOWER_THRESHOLD = this->paramGet_BATT_CELL_TEMP_LOWER_THRESHOLD(param_valid);
    const F64 BATT_CELL_TEMP_UPPER_THRESHOLD = this->paramGet_BATT_CELL_TEMP_UPPER_THRESHOLD(param_valid);

    // Face temp sensors
    for (FwIndexType i = 0; i < this->getNum_faceTempGet_OutputPorts(); i++) {
        F64 temperature = this->faceTempGet_out(i, condition);
        if (temperature < FACE_TEMP_LOWER_THRESHOLD) {
            this->log_WARNING_LO_FaceTemperatureBelowThreshold(i, temperature);
        } else if (temperature > FACE_TEMP_LOWER_THRESHOLD + ThermalManager::DEBOUNCE_ERROR) {
            this->log_WARNING_LO_FaceTemperatureBelowThreshold_ThrottleClear();
        }
        if (temperature > FACE_TEMP_UPPER_THRESHOLD) {
            this->log_WARNING_LO_FaceTemperatureAboveThreshold(i, temperature);
        } else if (temperature < FACE_TEMP_UPPER_THRESHOLD - ThermalManager::DEBOUNCE_ERROR) {
            this->log_WARNING_LO_FaceTemperatureAboveThreshold_ThrottleClear();
        }
    }

    // Battery cell temp sensors
    for (FwIndexType i = 0; i < this->getNum_battCellTempGet_OutputPorts(); i++) {
        F64 temperature = this->battCellTempGet_out(i, condition);
        if (temperature < BATT_CELL_TEMP_LOWER_THRESHOLD) {
            this->log_WARNING_LO_BatteryCellTemperatureBelowThreshold(i, temperature);
        } else if (temperature > BATT_CELL_TEMP_LOWER_THRESHOLD + ThermalManager::DEBOUNCE_ERROR) {
            this->log_WARNING_LO_BatteryCellTemperatureBelowThreshold_ThrottleClear();
        }
        if (temperature > BATT_CELL_TEMP_UPPER_THRESHOLD) {
            this->log_WARNING_LO_BatteryCellTemperatureAboveThreshold(i, temperature);
        } else if (temperature < BATT_CELL_TEMP_UPPER_THRESHOLD - ThermalManager::DEBOUNCE_ERROR) {
            this->log_WARNING_LO_BatteryCellTemperatureAboveThreshold_ThrottleClear();
        }
    }

    // Pico temp sensor
    this->picoTempGet_out(0, condition);
}

}  // namespace Components
