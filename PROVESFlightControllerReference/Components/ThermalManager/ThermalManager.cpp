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

ThermalManager::ThermalManager(const char* const compName) : ThermalManagerComponentBase(compName) {
    // Initialize vectors based on runtime port counts
    const FwIndexType numFaceTempPorts = this->getNum_faceTempGet_OutputPorts();
    const FwIndexType numBattCellTempPorts = this->getNum_battCellTempGet_OutputPorts();

    m_faceTempBelowActive.resize(numFaceTempPorts, false);
    m_faceTempAboveActive.resize(numFaceTempPorts, false);
    m_battCellTempBelowActive.resize(numBattCellTempPorts, false);
    m_battCellTempAboveActive.resize(numBattCellTempPorts, false);
}

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
        // Ensure index is within bounds of our state arrays
        FW_ASSERT(i < static_cast<FwIndexType>(this->m_faceTempBelowActive.size()));
        FW_ASSERT(i < static_cast<FwIndexType>(this->m_faceTempAboveActive.size()));

        F64 temperature = this->faceTempGet_out(i, condition);

        // Check if the temperature reading is valid before processing
        if (condition != Fw::Success::SUCCESS) {
            continue;
        }

        if (temperature < FACE_TEMP_LOWER_THRESHOLD) {
            if (!this->m_faceTempBelowActive[i]) {
                this->m_faceTempBelowActive[i] = true;
                this->log_WARNING_LO_FaceTemperatureBelowThreshold(i, temperature);
            }
        } else if (temperature > FACE_TEMP_LOWER_THRESHOLD + ThermalManager::DEBOUNCE_ERROR) {
            this->m_faceTempBelowActive[i] = false;
        }

        if (temperature > FACE_TEMP_UPPER_THRESHOLD) {
            if (!this->m_faceTempAboveActive[i]) {
                this->m_faceTempAboveActive[i] = true;
                this->log_WARNING_LO_FaceTemperatureAboveThreshold(i, temperature);
            }
        } else if (temperature < FACE_TEMP_UPPER_THRESHOLD - ThermalManager::DEBOUNCE_ERROR) {
            this->m_faceTempAboveActive[i] = false;
        }
    }

    // Battery cell temp sensors
    for (FwIndexType i = 0; i < this->getNum_battCellTempGet_OutputPorts(); i++) {
        // Ensure index is within bounds of our state arrays
        FW_ASSERT(i < static_cast<FwIndexType>(this->m_battCellTempBelowActive.size()));
        FW_ASSERT(i < static_cast<FwIndexType>(this->m_battCellTempAboveActive.size()));

        F64 temperature = this->battCellTempGet_out(i, condition);

        // Check if the temperature reading is valid before processing
        if (condition != Fw::Success::SUCCESS) {
            continue;
        }

        if (temperature < BATT_CELL_TEMP_LOWER_THRESHOLD) {
            if (!this->m_battCellTempBelowActive[i]) {
                this->m_battCellTempBelowActive[i] = true;
                this->log_WARNING_LO_BatteryCellTemperatureBelowThreshold(i, temperature);
            }
        } else if (temperature > BATT_CELL_TEMP_LOWER_THRESHOLD + ThermalManager::DEBOUNCE_ERROR) {
            this->m_battCellTempBelowActive[i] = false;
        }

        if (temperature > BATT_CELL_TEMP_UPPER_THRESHOLD) {
            if (!this->m_battCellTempAboveActive[i]) {
                this->m_battCellTempAboveActive[i] = true;
                this->log_WARNING_LO_BatteryCellTemperatureAboveThreshold(i, temperature);
            }
        } else if (temperature < BATT_CELL_TEMP_UPPER_THRESHOLD - ThermalManager::DEBOUNCE_ERROR) {
            this->m_battCellTempAboveActive[i] = false;
        }
    }

    // Pico temp sensor
    this->picoTempGet_out(0, condition);
}

}  // namespace Components
