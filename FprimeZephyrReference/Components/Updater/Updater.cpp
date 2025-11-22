// ======================================================================
// \title  Updater.cpp
// \author starchmd
// \brief  cpp file for Updater component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Updater/Updater.hpp"

#include <zephyr/dfu/mcuboot.h>

namespace Zephyr {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Updater ::Updater(const char* const compName) : UpdaterComponentBase(compName) {}

Updater ::~Updater() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Updater ::NEXT_BOOT_TEST_IMAGE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    int return_code = boot_request_upgrade(BOOT_UPGRADE_TEST);
    if (return_code != 0) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    } else {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    }
}

void Updater ::CONFIRM_NEXT_BOOT_IMAGE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    int return_code = boot_write_img_confirmed();
    if (return_code != 0) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    } else {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    }
}

}  // namespace Zephyr
