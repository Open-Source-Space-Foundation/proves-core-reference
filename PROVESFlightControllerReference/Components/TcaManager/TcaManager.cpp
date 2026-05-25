// ======================================================================
// \title  TcaManager.cpp
// \author moisesmata
// \brief  cpp file for TcaManager component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/TcaManager/TcaManager.hpp"

#include "config/FpConfig.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TcaManager ::TcaManager(const char* const compName) : TcaManagerComponentBase(compName) {}

TcaManager ::~TcaManager() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void TcaManager ::tcaError_handler(FwIndexType portNum) {
    // Toggle the mux reset to attempt to recover from the TCA error
    this->muxReset_out(0, Fw::Logic::HIGH);
    this->muxReset_out(0, Fw::Logic::LOW);

    // Report tcaManager started
    this->log_ACTIVITY_HI_MuxResetToggled();
}

}  // namespace Components
