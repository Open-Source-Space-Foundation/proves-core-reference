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
            this->evaluateTemperatureThreshold(i, temperature, this->faceAboveTemperatureThrottleActive[i],
                                               this->faceBelowTemperatureThrottleActive[i],
                                               Components::ThermalManager_TempSensorType::FACE);
        }
    }

    // Battery cell temp sensors
    for (FwIndexType i = 0; i < this->getNum_battCellTempGet_OutputPorts(); i++) {
        const F64 temperature = this->battCellTempGet_out(i, condition);
        if (condition == Fw::Success::SUCCESS) {  // Only evaluate thresholds if temperature reading was successful
            this->evaluateTemperatureThreshold(i, temperature, this->battCellAboveTemperatureThrottleActive[i],
                                               this->battCellBelowTemperatureThrottleActive[i],
                                               Components::ThermalManager_TempSensorType::BATTERY);
        }
    }

    // Pico temp sensor
    this->picoTempGet_out(0, condition);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

void ThermalManager::evaluateTemperatureThreshold(U32 idx,
                                                  F64 temperature,
                                                  bool& aboveTemperatureThrottleActive,
                                                  bool& belowTemperatureThrottleActive,
                                                  Components::ThermalManager_TempSensorType sensorType) {
    // Initialize parameter values
    Fw::ParamValid param_valid;
    F64 lowerThreshold = 0.0;
    F64 upperThreshold = 0.0;

    switch (sensorType) {
        case Components::ThermalManager_TempSensorType::FACE:
            lowerThreshold = this->paramGet_FACE_TEMP_LOWER_THRESHOLD(param_valid);
            upperThreshold = this->paramGet_FACE_TEMP_UPPER_THRESHOLD(param_valid);
            break;
        case Components::ThermalManager_TempSensorType::BATTERY:
            lowerThreshold = this->paramGet_BATT_CELL_TEMP_LOWER_THRESHOLD(param_valid);
            upperThreshold = this->paramGet_BATT_CELL_TEMP_UPPER_THRESHOLD(param_valid);
            break;
        default:
            FW_ASSERT(0, sensorType);  // Invalid temperature sensor type
    }

    // Check below temperature threshold
    if (!belowTemperatureThrottleActive && temperature < lowerThreshold) {
        belowTemperatureThrottleActive = true;
        aboveTemperatureThrottleActive = false;
        this->log_WARNING_LO_TemperatureBelowThreshold(sensorType, idx, temperature);
        return;
    }
    if (temperature > (lowerThreshold + ThermalManager::DEBOUNCE_ERROR)) {
        belowTemperatureThrottleActive = false;
    }

    // Check above temperature threshold
    if (!aboveTemperatureThrottleActive && temperature > upperThreshold) {
        aboveTemperatureThrottleActive = true;
        belowTemperatureThrottleActive = false;
        this->log_WARNING_LO_TemperatureAboveThreshold(sensorType, idx, temperature);
        return;
    }
    if (temperature < (upperThreshold - ThermalManager::DEBOUNCE_ERROR)) {
        aboveTemperatureThrottleActive = false;
    }
}

}  // namespace Components
