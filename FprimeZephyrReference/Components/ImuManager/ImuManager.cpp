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
    this->tlmWrite_Acceleration(this->accelerationRead_out(0));
    this->tlmWrite_AngularVelocity(this->angularVelocityRead_out(0));
    // this->tlmWrite_MagneticField(this->magneticFieldRead_out(0));
    this->tlmWrite_Temperature(this->temperatureRead_out(0));

    this->magneticFieldRead_out(0);
}
}  // namespace Components
