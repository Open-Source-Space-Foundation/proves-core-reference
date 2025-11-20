// ======================================================================
// \title  FsSpace.cpp
// \author starchmd
// \brief  cpp file for FsSpace component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/FsSpace/FsSpace.hpp"

#include "Os/FileSystem.hpp"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FsSpace ::FsSpace(const char* const compName) : FsSpaceComponentBase(compName) {}

FsSpace ::~FsSpace() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void FsSpace ::run_handler(FwIndexType portNum, U32 context) {
    FwSizeType freeBytes = 0;
    FwSizeType totalBytes = 0; 
    Os::FileSystem::Status status = Os::FileSystem::getFreeSpace("/prmDb.dat", totalBytes, freeBytes);
    if (status == Os::FileSystem::OP_OK) {
        this->tlmWrite_FreeSpace(freeBytes);
        this->tlmWrite_TotalSpace(totalBytes);
    }
}

}  // namespace Components
