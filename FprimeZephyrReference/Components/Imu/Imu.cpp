// ======================================================================
// \title  Imu.cpp
// \author aychar
// \brief  cpp file for Imu component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Imu/Imu.hpp"

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
    this->imuCallCount++;
    this->tlmWrite_ImuCalls(this->imuCallCount);
    this->log_ACTIVITY_HI_ImuTestEvent();
}

}  // namespace Components
