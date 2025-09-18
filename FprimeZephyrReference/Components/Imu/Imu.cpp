// ======================================================================
// \title  Imu.cpp
// \brief  cpp file for Imu component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Imu/Imu.hpp"

#include <Fw/Types/Assert.hpp>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Imu ::Imu(const char* const compName) : ImuComponentBase(compName) {}

Imu ::~Imu() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Imu ::run_handler(FwIndexType portNum, U32 context) {
    this->tlmWrite_Acceleration(this->readAcceleration_out(0));
    this->tlmWrite_AngularVelocity(this->readAngularVelocity_out(0));
    this->tlmWrite_MagneticField(this->readMagneticField_out(0));
    this->tlmWrite_Temperature(this->readTemperature_out(0));
}

}  // namespace Components
