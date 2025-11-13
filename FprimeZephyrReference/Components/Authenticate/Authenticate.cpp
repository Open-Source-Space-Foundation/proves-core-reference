// ======================================================================
// \title  Authenticate.cpp
// \author t38talon
// \brief  cpp file for Authenticate component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Authenticate/Authenticate.hpp"

const int HMAC_AUTHENTIFICATION = 0;

// TO DO: ADD TO THE DOWNLINK PATH FOR LORA AS WELL
// TO DO: REMOVE FROM REFDEPLOYMENT BC ITS IN THE COMCCSDS UPPER LEVEL (?????)

namespace Components {
// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Authenticate ::Authenticate(const char* const compName) : AuthenticateComponentBase(compName), sequenceNumber(0) {
    static constexpr const char* const sequenceNumberPath = "sequence_number.txt";

    U32 fileSequenceNumber = 0;
    bool loadedFromFile = false;

    Os::File::Status openStatus = this->m_sequenceNumberFile.open(sequenceNumberPath, Os::File::OPEN_READ);
    if (openStatus == Os::File::OP_OK) {
        FwSizeType size = static_cast<FwSizeType>(sizeof(fileSequenceNumber));
        FwSizeType expectedSize = size;
        Os::File::Status readStatus =
            this->m_sequenceNumberFile.read(reinterpret_cast<U8*>(&fileSequenceNumber), size, Os::File::WaitType::WAIT);
        this->m_sequenceNumberFile.close();
        loadedFromFile = (readStatus == Os::File::OP_OK) && (size == expectedSize);
    }

    if (!loadedFromFile) {
        fileSequenceNumber = 0;
        Os::File::Status createStatus = this->m_sequenceNumberFile.open(sequenceNumberPath, Os::File::OPEN_CREATE,
                                                                        Os::File::OverwriteType::NO_OVERWRITE);
        if (createStatus == Os::File::OP_OK) {
            const U8* buffer = reinterpret_cast<const U8*>(&fileSequenceNumber);
            FwSizeType size = static_cast<FwSizeType>(sizeof(fileSequenceNumber));
            (void)this->m_sequenceNumberFile.write(buffer, size, Os::File::WaitType::WAIT);
            this->m_sequenceNumberFile.close();
        }
    }

    this->sequenceNumber.store(fileSequenceNumber);
}

Authenticate ::~Authenticate() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

bool Authenticate ::PacketRequiresAuthentication(Fw::Buffer& data, const ComCfg::FrameContext& context) {
    (void)data;
    (void)context;
    // TODO: checks with the APID list to see if the packet requires authentication
    // If it does, return true
    // If it does not, return false
    // by default, return true if the APID is not in the APID list
    return false;
}

void Authenticate::computeHMAC(string& frameHeader, string& securityHeader, string& frameDataField, string& hmacKey) {
    // TODO: compute the HMAC of the packet
    // 1. Frame Header**: The CCSDS Space Packet Primary Header (6 bytes) - Extracted directly from the buffer at
    // bytes 0-5
    // 2. **Security Header**: SPI (2 bytes) + Sequence Number (4 bytes) + Reserved (2 bytes) = 8 bytes (bytes 6-13
    // of data field)
    // 3. **Frame Data Field**: The F Prime command packet payload (bytes 22-N)
    // compute the HMAC of the packet
    // return the HMAC
}

bool Authenticate::validateSequenceNumber(U32 received, U32 expected, U32 window) {
    // validate the sequence number by checking if it is within the window of the expected sequence number
    const U32 delta = received - expected;  // wraps naturally in U32 arithmetic
    if (delta > window) {
        this->log_WARNING_HI_SequenceNumberOutOfWindow(received, expected, window);
        return false;
    }
    return true;
}

bool compareHMAC(FW : buffer& hmac, FW : buffer& computedHMAC) {
    // compare the HMAC with the computed HMAC
    return hmac.equals(computedHMAC);
}

void Authenticate ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // Get packet APID and pass to PacketRequiresAuthentication
    ComCfg::Apid apid = context.get_apid();
    bool requiresAuthentication = this->PacketRequiresAuthentication(data, context);

    if (requiresAuthentication) {
        // Authenticate the packet

        // TO DO
        // get the SPI from the header
        const U32 spi = 0U;

        // TO DO
        // use the SPI to get the type of authentication and the key
        const AuthenticationConfig authConfig = this->lookupAuthenticationConfig(spi);
        const U32 type_authn = authConfig.type;
        const U32 key_authn = authConfig.key;

        // get the sequence number from the sequence number file
        U32 sequenceNumber = this->sequenceNumber.load();

        if (type_authn == HMAC_AUTHENTIFICATION) {
            // TO DO
            // get the frame header, security header, and frame data field from the packet
            // compute the HMAC of the packet
            const U8* raw = data.getData();
            const FwSizeType total = data.getSize();
            FW_ASSERT(total >= 6 + 8 + 8);

            const U8* frameHeader = raw;         // 6 bytes
            const U8* securityHeader = raw + 6;  // 8 bytes
            const U8* securityTrailer = raw + total - 8;
            const U8* commandPayload = raw + 14;
            FwSizeType commandLen = total - 14 - 8;

            Fw::Buffer computed_hmac = this->computeHMAC(frameHeader, securityHeader, commandPayload, key_authn);

            // get hmac from the security trailer
            const U8* received_hmac = securityTrailer + 8;

            // TO DO
            // validate the sequence number
            bool sequenceNumberValid = this->validateSequenceNumber(sequenceNumber, expectedSequenceNumber, window);
            if (!sequenceNumberValid) {
                // Authentication failed
                this->dataReturnOut_out(0, data, context);
                return;
            }

            // compare the HMAC with the computed HMAC
            bool hmacMatches = this->compareHMAC(received_hmac, computed_hmac);
            if (!hmacMatches) {
                // Authentication failed
                this->dataReturnOut_out(0, data, context);
                return;
            }
        } else {
            this->log_WARNING_HI_InvalidAuthenticationType(type_authn);
            this->dataReturnOut_out(0, data, context);
            return;
        }

    } else {
        // Strip the security headers and trailers from the packet
        // TO DO Double check this with the gds addition and the other components
        assert(data.getSize() >= 16);
        data.setData(data.getData() + 8);
        data.setSize(data.getSize() - 16);
        // Forward the packet to the SpacePacketDeframer
        this->dataOut_out(0, data, context);
    }
}

Authenticate::AuthenticationConfig Authenticate ::lookupAuthenticationConfig(U32 spi) const {
    (void)spi;
    AuthenticationConfig config{};
    config.type = HMAC_AUTHENTIFICATION;
    config.key = 0U;
    // TODO: fetch real authentication data for the provided SPI using the file!!!
    return config;
}

void Authenticate ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, data, context);
}

U32 Authenticate ::get_SequenceNumber() {
    static constexpr const char* const sequenceNumberPath = "sequence_number.txt";

    bool loadedFromFile = true;
    U32 fileSequenceNumber = 0;
    Os::File::Status openStatus = this->m_sequenceNumberFile.open(sequenceNumberPath, Os::File::OPEN_READ);

    if (openStatus == Os::File::OP_OK) {
        FwSizeType size = static_cast<FwSizeType>(sizeof(fileSequenceNumber));
        FwSizeType expectedSize = size;
        Os::File::Status readStatus =
            this->m_sequenceNumberFile.read(reinterpret_cast<U8*>(&fileSequenceNumber), size, Os::File::WaitType::WAIT);
        this->m_sequenceNumberFile.close();
        if ((readStatus != Os::File::OP_OK) || (size != expectedSize)) {
            loadedFromFile = false;
        }
    } else {
        loadedFromFile = false;
    }

    if (loadedFromFile) {
        this->sequenceNumber.store(fileSequenceNumber);
        return fileSequenceNumber;
    } else {
        fileSequenceNumber = this->sequenceNumber.load();
        return fileSequenceNumber;
    }
}
// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Authenticate ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    U32 fileSequenceNumber = this->get_SequenceNumber();

    this->log_ACTIVITY_HI_EmitSequenceNumber(fileSequenceNumber);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Authenticate ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    // Writes the sequence number to the file system
    static constexpr const char* const sequenceNumberPath = "sequence_number.txt";

    this->sequenceNumber.store(seq_num);

    bool persistSuccess = true;

    Os::File::Status openStatus =
        this->m_sequenceNumberFile.open(sequenceNumberPath, Os::File::OPEN_CREATE, Os::File::OverwriteType::OVERWRITE);
    if (openStatus == Os::File::OP_OK) {
        const U8* buffer = reinterpret_cast<const U8*>(&seq_num);
        FwSizeType size = static_cast<FwSizeType>(sizeof(seq_num));
        FwSizeType expectedSize = size;
        Os::File::Status writeStatus = this->m_sequenceNumberFile.write(buffer, size, Os::File::WaitType::WAIT);
        this->m_sequenceNumberFile.close();
        if ((writeStatus != Os::File::OP_OK) || (size != expectedSize)) {
            persistSuccess = false;
        }
    } else {
        persistSuccess = false;
    }

    this->log_ACTIVITY_HI_SetSequenceNumberSuccess(seq_num, persistSuccess);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Components
