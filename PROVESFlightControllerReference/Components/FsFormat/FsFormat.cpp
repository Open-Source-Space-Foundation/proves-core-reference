// ======================================================================
// \title  FsFormat.cpp
// \author nate
// \brief  cpp file for FsFormat component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/FsFormat/FsFormat.hpp"

#include <zephyr/fs/fs.h>
#include <zephyr/sys/util.h>

// The SD card disk backing the "/" littlefs mount (see Main.cpp).
#define STORAGE_DISK_NAME "SD"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FsFormat ::FsFormat(const char* const compName) : FsFormatComponentBase(compName) {}

FsFormat ::~FsFormat() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void FsFormat ::FORMAT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    int rc = fs_mkfs(FS_LITTLEFS, (uintptr_t)STORAGE_DISK_NAME, NULL, FS_MOUNT_FLAG_USE_DISK_ACCESS);

    if (rc < 0) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
