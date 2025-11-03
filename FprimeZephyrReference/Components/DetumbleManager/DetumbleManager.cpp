// ======================================================================
// \title  DetumbleManager.cpp
// \brief  cpp file for DetumbleManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/DetumbleManager/DetumbleManager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DetumbleManager ::DetumbleManager(const char* const compName) : DetumbleManagerComponentBase(compName) {}

DetumbleManager ::~DetumbleManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void DetumbleManager ::run_handler(FwIndexType portNum, U32 context) {
    // Read from Lsm6dsoManager
    this->accelerationGet_out(0);
    this->angularVelocityGet_out(0);
    this->temperatureGet_out(0);

    // Read from Lis2mdlManager
    this->magneticFieldGet_out(0);
}

bool DetumbleManager::executeControlStep() {
    Drv::MagneticField mgField = this->magneticFieldGet_out(0);
    if (this->prevMgField == this->EMPTY_MG_FIELD) {
        this->prevMgField = mgField;
    }

    Drv::DipoleMoment dpMoment = this->dipoleMomentGet_handler(0, mgField, this->prevMgField);
    // Then check if its null/empty whatever here, need to figure that out.
    this->prevMgField = mgField;

    // Then apply the dipole moment here, gonna have to figure that out.
    return true;
}
}  // namespace Components
