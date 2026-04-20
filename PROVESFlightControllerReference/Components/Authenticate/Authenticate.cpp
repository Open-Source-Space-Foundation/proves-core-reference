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

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Authenticate ::Authenticate(const char* const compName)
    : AuthenticateComponentBase(compName),
      m_packetParser(PacketParser()),
      m_sequenceNumberFilePath(),
      m_sequenceNumber(0),
      m_sequenceNumberWindow(0),
      m_bypassPacketsCount(0),
      m_rejectedPacketsCount(0) {}

Authenticate ::~Authenticate() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Authenticate ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    ComCfg::FrameContext contextOut = context;

    PacketParser::ParseResult rc = this->m_packetParser.parsePacket(
        data.getData(), data.getSize(), this->m_sequenceNumber, this->m_sequenceNumberWindow);
    switch (rc) {
        case PacketParser::ParseResult::Ok: {
            // Telemeter the updated sequence number
            this->m_sequenceNumber += 1;
            this->tlmWrite_CurrentSequenceNumber(this->m_sequenceNumber);

            // Write sequence number to persistent storage
            Os::File::Status status = this->writeSequenceNumber(this->m_sequenceNumber);
            if (status != Os::File::OP_OK) {
                // TODO(nateinaction): Decide what the appropriate behavior should be
            }

            break;
        }

        case PacketParser::ParseResult::Bypass: {
            // Telemeter the updated bypass packet count
            this->m_bypassPacketsCount += 1;
            this->tlmWrite_BypassPacketsCount(this->m_bypassPacketsCount);

            break;
        }

        case PacketParser::ParseResult::SequenceNumberValidationError: {
            // Log the sequence number validation failure
            this->log_WARNING_HI_SequenceNumberOutOfWindow(this->m_sequenceNumber, this->m_sequenceNumberWindow);

            // fall through
        }

        default: {
            // Telemeter the updated rejected packet count
            this->m_rejectedPacketsCount += 1;
            this->tlmWrite_RejectedPacketsCount(this->m_rejectedPacketsCount);

            // Log the packet rejection
            this->log_WARNING_HI_PacketRejected(static_cast<ParseResult::T>(rc));

            // Set frame context authenticated to false
            contextOut.set_authenticated(false);

            break;
        }
    }

    this->dataOut_out(0, data, contextOut);
}

void Authenticate ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, data, context);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Authenticate ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Read the sequence number from the file system
    U32 sequenceNumber = 0;
    Os::File::Status status = this->readSequenceNumber(sequenceNumber);
    if (status != Os::File::OP_OK) {
        // Return execution error response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Log the successful sequence number get
    this->log_ACTIVITY_HI_SequenceNumberGet(sequenceNumber);

    // Return success response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Authenticate ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    // Write the sequence number to the file system
    Os::File::Status status = this->writeSequenceNumber(seq_num);
    if (status != Os::File::OP_OK) {
        // Return execution error response
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Log the successful sequence number set
    this->log_ACTIVITY_HI_SequenceNumberSet(seq_num);

    // Return success response
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Public helper methods
// ----------------------------------------------------------------------

void Authenticate ::init(FwEnumStoreType instance) {
    Fw::ParamValid is_valid;

    // Call init from the base class
    AuthenticateComponentBase::init(instance);

    // Get the file path from the parameter
    this->m_sequenceNumberFilePath = this->paramGet_SEQ_NUM_FILE_PATH(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);

    // Get the sequence number from the file system
    U32 sequenceNumber;
    Os::File::Status status = this->readSequenceNumber(sequenceNumber);
    FW_ASSERT(status == Os::File::OP_OK);

    // Telemeter the current sequence number
    this->tlmWrite_CurrentSequenceNumber(sequenceNumber);

    // Get the sequence number window size from the parameter
    this->m_sequenceNumberWindow = this->paramGet_SEQ_NUM_WINDOW(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

Os::File::Status Authenticate ::readSequenceNumber(U32& value) {
    // Read the sequence number from the file system
    Os::File::Status status = Utilities::FileHelper::readFromFile(this->m_sequenceNumberFilePath.toChar(), value);
    if (status != Os::File::OP_OK) {
        // Log the failure to read the sequence number
        this->log_WARNING_HI_SequenceNumberReadFailed(static_cast<Os::FileStatus::T>(status));
    }

    if (status != Os::File::DOESNT_EXIST) {
        return status;
    }

    // If the sequence number file does not exist, write it to disk with the default value of 0
    return this->writeSequenceNumber(0);
}

Os::File::Status Authenticate ::writeSequenceNumber(const U32 value) {
    Os::File::Status status = Utilities::FileHelper::writeToFile(this->m_sequenceNumberFilePath.toChar(), value);
    if (status != Os::File::OP_OK) {
        // Log the failure to write the default sequence number
        this->log_WARNING_HI_SequenceNumberWriteFailed(static_cast<Os::FileStatus::T>(status));
    }

    return status;
}

}  // namespace Components
