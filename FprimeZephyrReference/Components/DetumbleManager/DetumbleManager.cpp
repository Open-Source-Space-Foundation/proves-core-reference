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
}  // namespace Components
