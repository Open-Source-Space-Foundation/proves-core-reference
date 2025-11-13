// ======================================================================
// \title  Authenticate.cpp
// \author t38talon
// \brief  cpp file for Authenticate component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Authenticate/Authenticate.hpp"

#include <Fw/Log/LogString.hpp>
#include <cstring>
#include <functional>

// Hardcoded Dictionary of Authentication Types
// could be put in a file, but since its linked to the actual code its simpleer to have it here

constexpr const char DEFAULT_AUTHENTICATION_TYPE[] = "HMAC";
constexpr const char DEFAULT_AUTHENTICATION_KEY[] = "0x65b32a18e0c63a347b56e8ae6c51358a";
constexpr const char SPI_DICT_PATH[] = "spi_dict.json";
constexpr const char SEQUENCE_NUMBER_PATH[] = "sequence_number.txt";

// TO DO: ADD TO THE DOWNLINK PATH FOR LORA AS WELL
// TO DO: REMOVE FROM REFDEPLOYMENT BC ITS IN THE COMCCSDS UPPER LEVEL (?????)

namespace Components {
// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Authenticate ::Authenticate(const char* const compName) : AuthenticateComponentBase(compName), sequenceNumber(0) {
    U32 fileSequenceNumber = 0;
    bool loadedFromFile = false;

    Os::File::Status openStatus = this->m_sequenceNumberFile.open(SEQUENCE_NUMBER_PATH, Os::File::OPEN_READ);
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
        Os::File::Status createStatus = this->m_sequenceNumberFile.open(SEQUENCE_NUMBER_PATH, Os::File::OPEN_CREATE,
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

Fw::Buffer Authenticate::computeHMAC(const U8* frameHeader,
                                     const U8* securityHeader,
                                     const U8* commandPayload,
                                     const std::string& key) {
    (void)frameHeader;
    (void)securityHeader;
    (void)commandPayload;
    (void)key;
    // TODO: implement real HMAC computation
    return Fw::Buffer();
}

bool Authenticate::validateSequenceNumber(U32 received, U32 expected) {
    // validate the sequence number by checking if it is within the window of the expected sequence number
    Fw::ParamValid valid;
    U32 window = paramGet_SEQ_NUM_WINDOW(valid);
    const U32 delta = received - expected;  // wraps naturally in U32 arithmetic
    if (delta > window) {
        this->log_WARNING_HI_SequenceNumberOutOfWindow(received, expected, window);
        return false;
    }
    return true;
}

bool Authenticate::compareHMAC(const U8* expected, const U8* actual, FwSizeType length) const {
    if (expected == nullptr || actual == nullptr) {
        return false;
    }
    return std::memcmp(expected, actual, static_cast<size_t>(length)) == 0;
}

void Authenticate ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    // assert that the packet length is a correct length for a CCSDS Space Packet
    FW_ASSERT(data.getSize() >= 6 + 8 + 8);
    FW_ASSERT(data.getSize() <=
              225 + 6 + 8 + 8);  // 225 is the maximum length of a F Prime command packet (TO DO double check this)

    // unpack all of the information
    const U8* raw = data.getData();
    const FwSizeType total = data.getSize();
    FW_ASSERT(total >= 6 + 8 + 8);
    const U8* frameHeader = raw;         // 6 bytes
    const U8* securityHeader = raw + 6;  // 8 bytes
    const U8* securityTrailer = raw + total - 8;
    const U8* commandPayload = raw + 14;
    U32 received_sequenceNumber = (static_cast<U32>(securityHeader[2]) << 24) |
                                  (static_cast<U32>(securityHeader[3]) << 16) |
                                  (static_cast<U32>(securityHeader[4]) << 8) | static_cast<U32>(securityHeader[5]);
    U32 received_hmac =
        (securityTrailer[0] << 24) | (securityTrailer[1] << 16) | (securityTrailer[2] << 8) | securityTrailer[3];

    // get spi from the security header
    const U32 spi = (static_cast<U32>(securityHeader[0]) << 8) | static_cast<U32>(securityHeader[1]);

    // Get packet APID and pass to PacketRequiresAuthentication
    ComCfg::Apid apid = context.get_apid();
    bool requiresAuthentication = this->PacketRequiresAuthentication(data, context);

    if (requiresAuthentication) {
        // Authenticate the packet

        // TO DO
        // use the SPI to get the type of authentication and the key
        const AuthenticationConfig authConfig = this->lookupAuthenticationConfig(spi);
        const std::string type_authn = authConfig.type;
        const std::string& key_authn = authConfig.key;

        if (type_authn == "HMAC") {
            // TO DO
            // get the frame header, security header, and frame data field from the packet
            // compute the HMAC of the packet
            const FwSizeType total = data.getSize();
            FW_ASSERT(total >= 6 + 8 + 8);

            bool sequenceNumberValid =
                this->validateSequenceNumber(received_sequenceNumber, this->get_SequenceNumber());
            if (!sequenceNumberValid) {
                this->dataReturnOut_out(0, data, context);
                return;
            }

            Fw::Buffer computedHmac = this->computeHMAC(frameHeader, securityHeader, commandPayload, key_authn);
            const U8* computedHmacData = computedHmac.getData();
            const FwSizeType computedHmacLength = computedHmac.getSize();

            const U8* receivedHmac = securityTrailer;
            constexpr FwSizeType receivedHmacLength = 8;

            if ((computedHmacData == nullptr) || (computedHmacLength < receivedHmacLength) ||
                !this->compareHMAC(receivedHmac, computedHmacData, receivedHmacLength)) {
                this->dataReturnOut_out(0, data, context);
                return;
            }

        } else {
            // Current event expects a U32; hash the string for now until the event is updated.
            const U32 hashedType = static_cast<U32>(std::hash<std::string>{}(type_authn));
            this->log_WARNING_HI_InvalidAuthenticationType(hashedType);
            this->dataReturnOut_out(0, data, context);
            return;
        }

    } else {
        // Strip the security headers and trailers from the packet
        // TO DO Double check this with the gds addition and the other components
        FW_ASSERT(data.getSize() >= 6 + 8 + 8);
        const FwSizeType total = data.getSize();
        const FwSizeType payloadLength = total - 6 - 8 - 8;

        const U8* src = data.getData();
        U8* dest = data.getData();
        std::memmove(dest + 6, src + 6 + 8, static_cast<size_t>(payloadLength));
        data.setSize(6 + payloadLength);
        this->dataOut_out(0, data, context);
        return;
    }

    this->dataOut_out(0, data, context);
}

Authenticate::AuthenticationConfig Authenticate ::lookupAuthenticationConfig(U32 spi) {
    AuthenticationConfig config{};
    config.type = DEFAULT_AUTHENTICATION_TYPE;
    config.key = DEFAULT_AUTHENTICATION_KEY;

    Os::File spiDictFile;
    Os::File::Status openStatus = spiDictFile.open(SPI_DICT_PATH, Os::File::OPEN_READ);
    if (openStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_InvalidSPI(spi);
        return config;
    }

    FwSizeType fileSize = 0;
    Os::File::Status sizeStatus = spiDictFile.size(fileSize);
    if (sizeStatus != Os::File::OP_OK || fileSize == 0) {
        spiDictFile.close();
        this->log_WARNING_HI_InvalidSPI(spi);
        return config;
    }

    std::string fileContents;
    fileContents.resize(static_cast<size_t>(fileSize), '\0');
    FwSizeType bytesToRead = fileSize;
    Os::File::Status readStatus =
        spiDictFile.read(reinterpret_cast<U8*>(&fileContents[0]), bytesToRead, Os::File::WaitType::WAIT);
    spiDictFile.close();
    if (readStatus != Os::File::OP_OK || bytesToRead != fileSize) {
        this->log_WARNING_HI_InvalidSPI(spi);
        return config;
    }

    return config;
}

void Authenticate ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, data, context);
}

U32 Authenticate ::get_SequenceNumber() {
    bool loadedFromFile = true;
    U32 fileSequenceNumber = 0;
    Os::File::Status openStatus = this->m_sequenceNumberFile.open(SEQUENCE_NUMBER_PATH, Os::File::OPEN_READ);

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

void Authenticate ::GET_KEY_FROM_SPI_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 spi) {
    const AuthenticationConfig authConfig = this->lookupAuthenticationConfig(spi);
    Fw::LogStringArg keyArg(authConfig.key.c_str());
    Fw::LogStringArg typeArg(authConfig.type.c_str());
    this->log_ACTIVITY_HI_EmitSpiKey(keyArg, typeArg);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Authenticate ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    // Writes the sequence number to the file system
    this->sequenceNumber.store(seq_num);

    bool persistSuccess = true;

    Os::File::Status openStatus = this->m_sequenceNumberFile.open(SEQUENCE_NUMBER_PATH, Os::File::OPEN_CREATE,
                                                                  Os::File::OverwriteType::OVERWRITE);
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
