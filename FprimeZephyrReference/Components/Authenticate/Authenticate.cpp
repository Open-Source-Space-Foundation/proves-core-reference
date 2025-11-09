// ======================================================================
// \title  Authenticate.cpp
// \author t38talon
// \brief  cpp file for Authenticate component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Authenticate/Authenticate.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Authenticate ::Authenticate(const char* const compName) : AuthenticateComponentBase(compName) {
    // We want to read the sequence number from the file system. If there is no sequence 
    // number file, we should create it with a default value of 0.
    Os::File sequenceNumberFile;
    Os::File::Status sequenceNumberFileStatus = sequenceNumberFile.open("sequence_number.txt", Os::File::OPEN_READ);
    if (sequenceNumberFileStatus != Os::File::OP_OK) {
        sequenceNumberFile.open("sequence_number.txt", Os::File::OPEN_CREATE);
        sequenceNumberFile.write("0", 1);
    }
    sequenceNumberFile.close();
}

Authenticate ::~Authenticate() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Authenticate ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // TODO
}

void Authenticate ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // TODO
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Authenticate ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Read Sequence Number from file system
    Os::File sequenceNumberFile;
    Os::File::Status sequenceNumberFileStatus = sequenceNumberFile.open("sequence_number.txt", Os::File::OPEN_READ);
    if (sequenceNumberFileStatus != Os::File::OP_OK) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    sequenceNumberFile.close();

    // read the sequence number from the file
    U32 sequenceNumber;
    sequenceNumberFile.read(sequenceNumber, 1);
    this->sequenceNumber = sequenceNumber;

    // emit the sequence number as an event
    this->emit_EmitSequenceNumber(sequenceNumber);
    
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Authenticate ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    // Writes the sequence number to the file system
    Os::File sequenceNumberFile;
    Os::File::Status sequenceNumberFileStatus = sequenceNumberFile.open("sequence_number.txt", Os::File::OPEN_WRITE);
    if (sequenceNumberFileStatus != Os::File::OP_OK) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        this->emit_SetSequenceNumberSuccess(seq_num, false);
        return;
    }
    sequenceNumberFile.write(seq_num, 1);
    sequenceNumberFile.close();

    this->emit_SetSequenceNumberSuccess(seq_num, true);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
