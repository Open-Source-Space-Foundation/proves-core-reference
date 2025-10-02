// ======================================================================
// \title  BootloaderTrigger.cpp
// \author aychar
// \brief  cpp file for BootloaderTrigger component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/BootloaderTrigger/BootloaderTrigger.hpp"

#include <pico/bootrom.h>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

BootloaderTrigger ::BootloaderTrigger(const char* const compName) : BootloaderTriggerComponentBase(compName) {}

BootloaderTrigger ::~BootloaderTrigger() {}

// In the future this command/component should either be removed entirely, or some kind of protection
// should be added to ensure this can't be ran when the satellite is in space
void BootloaderTrigger ::TRIGGER_BOOTLOADER_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    reset_usb_boot(0, 0);
}

}  // namespace Components
