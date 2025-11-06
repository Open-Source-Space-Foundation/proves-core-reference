// ======================================================================
// \title  LightSensor.cpp
// \author robertpendergrast
// \brief  cpp file for LightSensor component implementation class
// ======================================================================



#include "FprimeZephyrReference/Components/LightSensor/LightSensor.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

LightSensor ::LightSensor(const char* const compName) : LightSensorComponentBase(compName) {}

LightSensor ::~LightSensor() {}

void LightSensor ::ConfigureSensor(const struct device* dev) {
    this->m_dev = dev;
    if (this->m_dev == nullptr) {
        this->log_WARNING_HI_LightSensorError();
        return;
    }

    this->m_configured = true;
}

void LightSensor ::ReadData() {
    // TODO
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void LightSensor ::run_handler(FwIndexType portNum, U32 context) {

    this->ReadData();
    this->tlmWrite_RawLightData(this->m_RawLightData);
    this->tlmWrite_IRLightData(this->m_IRLightData);
    this->tlmWrite_ALSLightData(this->m_ALSLightData);
}

}  // namespace Components
