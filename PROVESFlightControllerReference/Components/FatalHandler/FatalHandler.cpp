// ======================================================================
// \title  FatalHandlerImpl.cpp
// \author mstarch
// \brief  cpp file for FatalHandler component implementation class
//
// \copyright
// Copyright 2009-2015, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#include <Fw/FPrimeBasicTypes.hpp>
#include <Fw/Logger/Logger.hpp>
#include <Os/Task.hpp>
#include <PROVESFlightControllerReference/Components/FatalHandler/FatalHandler.hpp>

#include <zephyr/sys/reboot.h>

namespace Components {

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

FatalHandler ::FatalHandler(const char* const compName) : FatalHandlerComponentBase(compName) {}

FatalHandler ::~FatalHandler() {}

void FatalHandler::reboot() {
    sys_reboot(SYS_REBOOT_WARM);

    // Only reached if the warm reboot returns.
    sys_reboot(SYS_REBOOT_COLD);
}

void FatalHandler::FatalReceive_handler(const FwIndexType portNum, FwEventIdType Id) {
    Fw::Logger::log("FATAL %" PRI_FwEventIdType " handled.\n", Id);
    // Stop petting the external watchdog.
    this->stopWatchdog_out(0);
    // Short delay so the FATAL log can drain, then force a reboot so the reset
    // does not depend on the external watchdog.
    Os::Task::delay(Fw::TimeInterval(0, 1000));  // 1 ms
    this->reboot();
}

}  // namespace Components
