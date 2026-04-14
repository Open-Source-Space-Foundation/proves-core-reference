// ======================================================================
// \title  Authenticate.cpp
// \brief  cpp file for Authenticate component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/Authenticate/Authenticate.hpp"

#include <psa/crypto.h>

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Log/LogString.hpp>
#include <iomanip>

// Include generated header with default key (generated at build time)
#include "AuthDefaultKey.h"

// File path for storing the sequence number persistently
// TODO(nateinaction): Move to parameter
constexpr const char SEQUENCE_NUMBER_PATH[] = "//sequence_number.txt";

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Authenticate ::Authenticate(const char* const compName) : AuthenticateComponentBase(compName), m_sequenceNumber(0) {}

Authenticate ::~Authenticate() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Authenticate ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    U8* dataBuffer = data.getData();
    FwSizeType dataSize = data.getSize();

    int rc =
        this->m_packetParser.parsePacket(dataBuffer, dataSize, this->m_sequenceNumber, this->m_sequenceNumberWindow);
    if (rc == PacketParser::PARSE_ERROR) {
        this->log_WARNING_HI_InvalidHeader(context.get_apid());
        this->rejectPacket(data, context);
        return;
    } else if (rc == PacketParser::CRYPTO_ERROR) {
        this->log_WARNING_HI_CryptoComputationError(context.get_apid());
        this->rejectPacket(data, context);
        return;
    } else if (rc == PacketParser::REJECT_PACKET) {
        this->log_WARNING_HI_PacketRejected();
        this->rejectPacket(data, context);
        return;
    }

    this->m_sequenceNumber = this->m_sequenceNumber + 1;
    this->writeSequenceNumber(SEQUENCE_NUMBER_PATH, this->m_sequenceNumber);
    this->tlmWrite_CurrentSequenceNumber(this->m_sequenceNumber);

    U32 newCount = this->m_authenticatedPacketsCount.fetch_add(1) + 1;
    this->tlmWrite_AuthenticatedPacketsCount(newCount);

    this->dataOut_out(0, data, context);
}

void Authenticate ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, data, context);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Authenticate ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    U32 fileSequenceNumber = this->readSequenceNumber(SEQUENCE_NUMBER_PATH);

    this->log_ACTIVITY_HI_EmitSequenceNumber(fileSequenceNumber);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Authenticate ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    // Writes the sequence number to the file system
    this->writeSequenceNumber(SEQUENCE_NUMBER_PATH, seq_num);

    this->log_ACTIVITY_HI_SetSequenceNumberSuccess(seq_num, true);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void Authenticate ::init(FwEnumStoreType instance) {
    // Call init from the base class
    AuthenticateComponentBase::init(instance);

    // Init the sequence number
    U32 sequenceNumber = this->readSequenceNumber(SEQUENCE_NUMBER_PATH);
    this->m_sequenceNumber = sequenceNumber;
    this->tlmWrite_CurrentSequenceNumber(sequenceNumber);

    // Init the window parameter
    Fw::ParamValid valid;
    this->m_sequenceNumberWindow = this->paramGet_SEQ_NUM_WINDOW(valid);
    // TODO(nateinaction): Check param validity and handle case where param is invalid
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

U32 Authenticate ::readSequenceNumber(const char* filepath) {
    U32 value = 0;
    Os::File::Status status = Utilities::FileHelper::readFromFile(filepath, value);
    if (status != Os::File::OP_OK) {
        // TODO(nateinaction): do we want to set sequence number to 0 if we fail to read it? or should we handle this
        // differently?
        // TODO(nateinaction): status of this write is never checked
        Utilities::FileHelper::writeToFile(filepath, value);
    }
    return value;
}

U32 Authenticate ::writeSequenceNumber(const char* filepath, U32 value) {
    // Copy value to buffer to avoid type punning
    // TODO(nateinaction): status of this write is never checked
    Utilities::FileHelper::writeToFile(filepath, value);
    // TODO(nateinaction): do we need to return the value? It is passed in as an argument so the caller already has it
    return value;
}

void Authenticate::rejectPacket(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->log_WARNING_HI_PacketRejected();
    U32 newCount = this->m_rejectedPacketsCount.fetch_add(1) + 1;
    this->tlmWrite_RejectedPacketsCount(newCount);

    // Copy the context and set authenticated to false for the rejected packet
    ComCfg::FrameContext contextOut = context;
    contextOut.set_authenticated(false);

    // if the packet is rejected we no longer pass it down the comm stack

    this->dataReturnOut_out(0, data, contextOut);
}

}  // namespace Components
