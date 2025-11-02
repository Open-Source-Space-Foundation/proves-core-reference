// ======================================================================
// \title  CameraManager.cpp
// \author robertpendergrast
// \brief  cpp file for CameraManager component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/CameraManager/CameraManager.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CameraManager ::CameraManager(const char* const compName) : CameraManagerComponentBase(compName) {}

CameraManager ::~CameraManager() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void CameraManager ::TAKE_IMAGE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {

    // Prepare the "snap" command to send over UART via out_port
    const U8 size = sizeof(this->snapArray);

    // Create a buffer that references this array
    Fw::Buffer snapBuffer(this->snapArray, size);

    // Send the buffer via out_port, check/send status if needed
    Drv::ByteStreamStatus sendStatus = this->out_port_out(0, snapBuffer);
    
    if (sendStatus != Drv::ByteStreamStatus::OP_OK) {
        this->log_WARNING_HI_TakeImageError();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    else {
        this->log_ACTIVITY_HI_PictureTaken();
    }
    
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace FprimeZephyrReference
