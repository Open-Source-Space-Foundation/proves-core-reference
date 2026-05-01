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
        const F64 temperature = this->faceTempGet_out(i, condition);
        if (condition == Fw::Success::SUCCESS) {  // Only evaluate thresholds if temperature reading was successful
            this->evaluateTemperatureThreshold(i, temperature, FACE_TEMP_LOWER_THRESHOLD, FACE_TEMP_UPPER_THRESHOLD,
                                               this->faceTempBelowThrottleActive[i],
                                               this->faceTempAboveThrottleActive[i],
                                               &ThermalManager::log_WARNING_LO_FaceTemperatureBelowThreshold,
                                               &ThermalManager::log_WARNING_LO_FaceTemperatureAboveThreshold);
        }
    }

    // Battery cell temp sensors
    for (FwIndexType i = 0; i < this->getNum_battCellTempGet_OutputPorts(); i++) {
        const F64 temperature = this->battCellTempGet_out(i, condition);
        if (condition == Fw::Success::SUCCESS) {  // Only evaluate thresholds if temperature reading was successful
            this->evaluateTemperatureThreshold(i, temperature, BATT_CELL_TEMP_LOWER_THRESHOLD,
                                               BATT_CELL_TEMP_UPPER_THRESHOLD, this->battCellTempBelowThrottleActive[i],
                                               this->battCellTempAboveThrottleActive[i],
                                               &ThermalManager::log_WARNING_LO_BatteryCellTemperatureBelowThreshold,
                                               &ThermalManager::log_WARNING_LO_BatteryCellTemperatureAboveThreshold);
        }
    }

    // Pico temp sensor
    this->picoTempGet_out(0, condition);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void ThermalManager::evaluateTemperatureThreshold(FwIndexType idx,
                                                  F64 temperature,
                                                  F64 lowerThreshold,
                                                  F64 upperThreshold,
                                                  bool& belowThrottleActive,
                                                  bool& aboveThrottleActive,
                                                  LogFn logBelow,
                                                  LogFn logAbove) {
    if (temperature < lowerThreshold) {
        if (!belowThrottleActive) {
            belowThrottleActive = true;
            (this->*logBelow)(static_cast<U32>(idx), static_cast<F32>(temperature));
        }
    } else if (temperature > lowerThreshold + ThermalManager::DEBOUNCE_ERROR) {
        belowThrottleActive = false;
    }

    if (temperature > upperThreshold) {
        if (!aboveThrottleActive) {
            aboveThrottleActive = true;
            (this->*logAbove)(static_cast<U32>(idx), static_cast<F32>(temperature));
        }
    } else if (temperature < upperThreshold - ThermalManager::DEBOUNCE_ERROR) {
        aboveThrottleActive = false;
    }
}

}  // namespace Components
