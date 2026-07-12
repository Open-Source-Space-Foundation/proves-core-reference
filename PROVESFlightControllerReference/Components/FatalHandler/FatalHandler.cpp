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
    // Use Zephyr to reboot the system.
    // https://docs.zephyrproject.org/apidoc/latest/reboot_8h.html#a18abe5d5b8089e8429c25bafa5e76d3d
    // Attempt a warm reboot first.
    sys_reboot(SYS_REBOOT_WARM);

    // Attempt a cold reboot if the warm reboot somehow returns/fails.
    sys_reboot(SYS_REBOOT_COLD);
}

void FatalHandler::FatalReceive_handler(const FwIndexType portNum, FwEventIdType Id) {
    Fw::Logger::log("FATAL %" PRI_FwEventIdType " handled.\n", Id);
    // Stop petting the external hardware watchdog. This is kept as belt-and-braces: on
    // this deployment there is no Zephyr software watchdog, so the external HW watchdog
    // (Components.Watchdog, rateGroup1Hz member) is the nominal reset path once pets stop.
    this->stopWatchdog_out(0);
    // Do not rely solely on the external watchdog to eventually starve and reset the board
    // (it may be absent/disconnected on a bench, or its timeout may be long). Delay briefly
    // to allow the FATAL log/event to drain, then force a reboot directly so a real reset is
    // guaranteed regardless of the external watchdog's state.
    Os::Task::delay(Fw::TimeInterval(
        0, 1000));  // 1 ms (TimeInterval is seconds, microseconds); best-effort log drain before forced reboot
    this->reboot();
}

}  // namespace Components
