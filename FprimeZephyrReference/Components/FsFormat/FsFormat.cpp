// ======================================================================
// \title  FsFormat.cpp
// \author nate
// \brief  cpp file for FsFormat component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/FsFormat/FsFormat.hpp"

// #include <zephyr/kernel.h>
// #include <zephyr/device.h>
#include <zephyr/fs/fs.h>
// #include <zephyr/storage/flash_map.h>
// #include <zephyr/storage/disk_access.h>
// #include <ff.h>

#define MKFS_FS_TYPE FS_FATFS

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FsFormat ::FsFormat(const char* const compName) : FsFormatComponentBase(compName) {}

FsFormat ::~FsFormat() {}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void FsFormat ::configure(const int partition_id) {
    this->m_partition_id = partition_id;
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void FsFormat ::FORMAT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    int rc = fs_mkfs(MKFS_FS_TYPE, (uintptr_t)this->m_partition_id, NULL, 0);

    if (rc < 0) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
