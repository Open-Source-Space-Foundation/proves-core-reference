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

Authenticate ::Authenticate(const char* const compName) : AuthenticateComponentBase(compName), sequenceNumber(0) {
    // We want to read the sequence number from the file system. If there is no sequence
    // // number file, we should create it with a default value of 0.
    // Os::File sequenceNumberFile;
    // Os::File::Status sequenceNumberFileStatus = sequenceNumberFile.open("sequence_number.txt", Os::File::OPEN_READ);
    // if (sequenceNumberFileStatus != Os::File::OP_OK) {
    //     sequenceNumberFile.open("sequence_number.txt", Os::File::OPEN_CREATE);
    //     sequenceNumberFile.write("0", 1);
    // }
    // sequenceNumberFile.close();
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
    static constexpr const char* const sequenceNumberPath = "sequence_number.txt";

    Os::File::Status openStatus = this->m_sequenceNumberFile.open(sequenceNumberPath, Os::File::OPEN_READ);
    if (openStatus != Os::File::OP_OK) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    U32 fileSequenceNumber = 0;
    FwSizeType size = static_cast<FwSizeType>(sizeof(fileSequenceNumber));
    FwSizeType expectedSize = size;
    Os::File::Status readStatus =
        this->m_sequenceNumberFile.read(reinterpret_cast<U8*>(&fileSequenceNumber), size, Os::File::WaitType::WAIT);
    this->m_sequenceNumberFile.close();

    if ((readStatus != Os::File::OP_OK) || (size != expectedSize)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    this->sequenceNumber.store(fileSequenceNumber);
    this->log_ACTIVITY_HI_EmitSequenceNumber(fileSequenceNumber);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Authenticate ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    // Writes the sequence number to the file system
    static constexpr const char* const sequenceNumberPath = "sequence_number.txt";

    Os::File::Status openStatus =
        this->m_sequenceNumberFile.open(sequenceNumberPath, Os::File::OPEN_CREATE, Os::File::OverwriteType::OVERWRITE);
    if (openStatus != Os::File::OP_OK) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        this->log_ACTIVITY_HI_SetSequenceNumberSuccess(seq_num, false);
        return;
    }

    const U8* buffer = reinterpret_cast<const U8*>(&seq_num);
    FwSizeType size = static_cast<FwSizeType>(sizeof(seq_num));
    FwSizeType expectedSize = size;
    Os::File::Status writeStatus = this->m_sequenceNumberFile.write(buffer, size, Os::File::WaitType::WAIT);
    this->m_sequenceNumberFile.close();

    if ((writeStatus != Os::File::OP_OK) || (size != expectedSize)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        this->log_ACTIVITY_HI_SetSequenceNumberSuccess(seq_num, false);
        return;
    }
    this->sequenceNumber.store(seq_num);

    this->log_ACTIVITY_HI_SetSequenceNumberSuccess(seq_num, true);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
