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

    // Face temp sensors
    for (FwIndexType i = 0; i < this->getNum_faceTempGet_OutputPorts(); i++) {
        const F64 temperature = this->faceTempGet_out(i, condition);
        if (condition == Fw::Success::SUCCESS) {  // Only evaluate thresholds if temperature reading was successful
            this->evaluateTemperatureThreshold(i, temperature, this->faceTempThrottleActive[i],
                                               Components::ThermalManager_TempSensorType::FACE);
        }
    }

    // Battery cell temp sensors
    for (FwIndexType i = 0; i < this->getNum_battCellTempGet_OutputPorts(); i++) {
        const F64 temperature = this->battCellTempGet_out(i, condition);
        if (condition == Fw::Success::SUCCESS) {  // Only evaluate thresholds if temperature reading was successful
            this->evaluateTemperatureThreshold(i, temperature, this->battCellTempThrottleActive[i],
                                               Components::ThermalManager_TempSensorType::BATTERY);
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
                                                  bool& throttleActive,
                                                  Components::ThermalManager_TempSensorType sensorType) {
    Fw::ParamValid param_valid;
    F64 lowerThreshold = 0.0;
    F64 upperThreshold = 0.0;

    if (sensorType == Components::ThermalManager_TempSensorType::FACE) {
        lowerThreshold = this->paramGet_FACE_TEMP_LOWER_THRESHOLD(param_valid);
        upperThreshold = this->paramGet_FACE_TEMP_UPPER_THRESHOLD(param_valid);
    } else {
        lowerThreshold = this->paramGet_BATT_CELL_TEMP_LOWER_THRESHOLD(param_valid);
        upperThreshold = this->paramGet_BATT_CELL_TEMP_UPPER_THRESHOLD(param_valid);
    }

    if (!throttleActive && temperature < lowerThreshold) {
        throttleActive = true;
        this->log_WARNING_LO_TemperatureBelowThreshold(sensorType, static_cast<U32>(idx), temperature);
        return;
    }

    if (!throttleActive && temperature > upperThreshold) {
        throttleActive = true;
        this->log_WARNING_LO_TemperatureAboveThreshold(sensorType, static_cast<U32>(idx), temperature);
        return;
    }

    if (throttleActive && temperature > (lowerThreshold + ThermalManager::DEBOUNCE_ERROR) &&
        temperature < (upperThreshold - ThermalManager::DEBOUNCE_ERROR)) {
        throttleActive = false;
    }
}

}  // namespace Components
