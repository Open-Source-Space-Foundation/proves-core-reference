// ======================================================================
// \title  ADCS.cpp
// \brief  cpp file for ADCS component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ADCS/ADCS.hpp"

#include <Fw/Types/Assert.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ADCS::ADCS(const char* const compName) : ADCSComponentBase(compName) {}

ADCS::~ADCS() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ADCS::run_handler(FwIndexType portNum, U32 context) {
    Fw::Success condition;

    // Visible light
    for (FwIndexType i = 0; i < this->getNum_visibleLightGet_OutputPorts(); i++) {
        this->visibleLightGet_out(i, condition);
    }

    // Infra-red light
    for (FwIndexType i = 0; i < this->getNum_infraRedLightGet_OutputPorts(); i++) {
        this->infraRedLightGet_out(i, condition);
    }

    // Ambient light
    for (FwIndexType i = 0; i < this->getNum_ambientLightGet_OutputPorts(); i++) {
        this->ambientLightGet_out(i, condition);
    }
}

}  // namespace Components
