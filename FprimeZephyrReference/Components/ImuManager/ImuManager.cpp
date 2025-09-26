// ======================================================================
// \title  ImuManager.cpp
// \brief  cpp file for ImuManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/ImuManager/ImuManager.hpp"

#include <Fw/Types/Assert.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ImuManager ::ImuManager(const char* const compName) : ImuManagerComponentBase(compName) {}

ImuManager ::~ImuManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ImuManager ::run_handler(FwIndexType portNum, U32 context) {
    // Read from Lsm6dsoManager
    this->accelerationRead_out(0);
    this->angularVelocityRead_out(0);
    this->temperatureRead_out(0);

    // Read from Lis2mdlManager
    this->magneticFieldRead_out(0);
}
}  // namespace Components
