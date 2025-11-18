// ======================================================================
// \title  Authenticate.cpp
// \author Ines
// \brief  cpp file for Authenticate component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Authenticate/Authenticate.hpp"

#include <psa/crypto.h>

#include <Fw/Log/LogString.hpp>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <sstream>
#include <vector>

#include <zephyr/kernel.h>

// Hardcoded Dictionary of Authentication Types
// could be put in a file, but since its linked to the actual code its simpleer to have it here

constexpr const char DEFAULT_AUTHENTICATION_TYPE[] = "HMAC";
constexpr const char DEFAULT_AUTHENTICATION_KEY[] = "0x55b32a18e0c63a347b56e8ae6c51358a";
constexpr const char SPI_DICT_PATH[] = "//spi_dict.txt";
constexpr const char SEQUENCE_NUMBER_PATH[] = "//sequence_number.txt";
constexpr const char SPI_DICT_DELIMITER[] = " | ";

/// Types of A
constexpr const int HMAC = 0;
constexpr const int NONE = 1;

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

Fw::Buffer Authenticate::computeHMAC(const U8* securityHeader,
                                     const FwSizeType securityHeaderLength,
                                     const U8* commandPayload,
                                     const FwSizeType commandPayloadLength,
                                     const std::string& key) {
    // Initialize PSA crypto (idempotent, safe to call multiple times)
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        U32 statusU32 = static_cast<U32>(status);
        this->log_WARNING_HI_CryptoComputationError(statusU32);
        return Fw::Buffer();
    }

    // Parse key from hex string (with or without "0x" prefix) or use as raw bytes
    std::vector<U8> keyBytes;
    std::string hexKey = key;

    // Remove "0x" prefix if present
    if (key.length() > 2 && key.substr(0, 2) == "0x") {
        hexKey = key.substr(2);
    }

    // Check if all characters are hex digits
    bool isHexString = true;
    for (char c : hexKey) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            isHexString = false;
            break;
        }
    }

    // Parse as hex string if valid, otherwise use as raw bytes
    if (isHexString && hexKey.length() > 0 && hexKey.length() % 2 == 0) {
        for (size_t i = 0; i < hexKey.length(); i += 2) {
            std::string byteStr = hexKey.substr(i, 2);
            keyBytes.push_back(static_cast<U8>(std::stoul(byteStr, nullptr, 16)));
        }
    } else {
        keyBytes.assign(key.begin(), key.end());
    }

    // Combine security header and command payload into a single input buffer
    const size_t totalInputLength = static_cast<size_t>(securityHeaderLength + commandPayloadLength);
    std::vector<U8> inputBuffer;
    inputBuffer.reserve(totalInputLength);
    inputBuffer.insert(inputBuffer.end(), securityHeader, securityHeader + securityHeaderLength);
    inputBuffer.insert(inputBuffer.end(), commandPayload, commandPayload + commandPayloadLength);

    // Implement HMAC-SHA-256 manually using PSA hash API (RFC 2104)
    // HMAC(k, m) = H(k XOR opad || H(k XOR ipad || m))
    const size_t blockSize = 64;  // SHA-256 block size
    const U8 ipad = 0x36;
    const U8 opad = 0x5C;
    U8 macOutput[32];
    size_t macOutputLength = 0;

    // Prepare key: pad with zeros or hash if longer than block size
    std::vector<U8> preparedKey(blockSize, 0);
    if (keyBytes.size() <= blockSize) {
        std::memcpy(preparedKey.data(), keyBytes.data(), keyBytes.size());
    } else {
        // Hash key if longer than block size
        psa_hash_operation_t hashOp = PSA_HASH_OPERATION_INIT;
        status = psa_hash_setup(&hashOp, PSA_ALG_SHA_256);
        if (status != PSA_SUCCESS) {
            U32 statusU32 = static_cast<U32>(status);
            this->log_WARNING_HI_CryptoComputationError(statusU32);
            return Fw::Buffer();
        }
        status = psa_hash_update(&hashOp, keyBytes.data(), keyBytes.size());
        if (status == PSA_SUCCESS) {
            size_t hashLen = 0;
            status = psa_hash_finish(&hashOp, preparedKey.data(), blockSize, &hashLen);
        }
        if (status != PSA_SUCCESS) {
            U32 statusU32 = static_cast<U32>(status);
            this->log_WARNING_HI_CryptoComputationError(statusU32);
            return Fw::Buffer();
        }
    }

    // Compute inner hash: H(k XOR ipad || m)
    std::vector<U8> innerKey(blockSize);
    for (size_t i = 0; i < blockSize; i++) {
        innerKey[i] = preparedKey[i] ^ ipad;
    }

    psa_hash_operation_t innerHash = PSA_HASH_OPERATION_INIT;
    status = psa_hash_setup(&innerHash, PSA_ALG_SHA_256);
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&innerHash, innerKey.data(), blockSize);
    }
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&innerHash, inputBuffer.data(), inputBuffer.size());
    }
    U8 innerHashOutput[32];
    size_t innerHashLen = 0;
    if (status == PSA_SUCCESS) {
        status = psa_hash_finish(&innerHash, innerHashOutput, sizeof(innerHashOutput), &innerHashLen);
    }

    if (status != PSA_SUCCESS) {
        U32 statusU32 = static_cast<U32>(status);
        this->log_WARNING_HI_CryptoComputationError(statusU32);
        return Fw::Buffer();
    }

    // Compute outer hash: H(k XOR opad || innerHash)
    std::vector<U8> outerKey(blockSize);
    for (size_t i = 0; i < blockSize; i++) {
        outerKey[i] = preparedKey[i] ^ opad;
    }

    psa_hash_operation_t outerHash = PSA_HASH_OPERATION_INIT;
    status = psa_hash_setup(&outerHash, PSA_ALG_SHA_256);
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&outerHash, outerKey.data(), blockSize);
    }
    if (status == PSA_SUCCESS) {
        status = psa_hash_update(&outerHash, innerHashOutput, innerHashLen);
    }
    if (status == PSA_SUCCESS) {
        status = psa_hash_finish(&outerHash, macOutput, sizeof(macOutput), &macOutputLength);
    }

    if (status != PSA_SUCCESS) {
        U32 statusU32 = static_cast<U32>(status);
        this->log_WARNING_HI_CryptoComputationError(statusU32);
        return Fw::Buffer();
    }

    // Allocate a buffer for the HMAC result (we only need 16 bytes per CCSDS spec)
    // But we computed 32 bytes (full SHA-256), so we'll take the first 16
    const size_t hmacOutputLength = 16;  // CCSDS 355.0-B-2 specifies 16 bytes
    U8* hmacData = new U8[hmacOutputLength];
    if (hmacData == nullptr) {
        return Fw::Buffer();
    }
    std::memcpy(hmacData, macOutput, hmacOutputLength);

    // Create Fw::Buffer with the HMAC data (context 0 for now)
    return Fw::Buffer(hmacData, static_cast<FwSizeType>(hmacOutputLength), 0);
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
    ComCfg::FrameContext contextOut = context;

    // 34 = 12 (data) + 6 (security header) + 16 (security trailer)
    if (data.getSize() < 34) {
        // return the packet, set to unauthenticated
        this->log_WARNING_HI_PacketTooShort(data.getSize());
        contextOut.set_authenticated(0);
        this->dataOut_out(0, data, context);
        return;
    }

    // Take the first 6 bytes as the security header
    unsigned char securityHeader[6];
    std::memcpy(securityHeader, data.getData(), 6);
    // increment the pointer to the data to point to the rest of the packet
    data.setData(data.getData() + 6);
    data.setSize(data.getSize() - 6);

    // now we get the footer (last 16 bytes)
    unsigned char securityTrailer[16];
    std::memcpy(securityTrailer, data.getData() + data.getSize() - 16, 16);
    // decrement the size of the data to remove the footer
    data.setSize(data.getSize() - 16);

    // the first two bytes are the SPI
    U32 spi = (static_cast<U32>(securityHeader[0]) << 8) | static_cast<U32>(securityHeader[1]);

    // the next four bytes are the sequence number
    U32 sequenceNumber = (static_cast<U32>(securityHeader[2]) << 24) | (static_cast<U32>(securityHeader[3]) << 16) |
                         (static_cast<U32>(securityHeader[4]) << 8) | static_cast<U32>(securityHeader[5]);

    const AuthenticationConfig authConfig = this->lookupAuthenticationConfig(spi);
    const std::string type_authn = authConfig.type;
    const std::string& key_authn = authConfig.key;

    // check the sequence number is valid
    // cast trailer to U32
    bool sequenceNumberValid = this->validateSequenceNumber(sequenceNumber, this->get_SequenceNumber());
    if (!sequenceNumberValid) {
        contextOut.set_authenticated(0);
        this->dataOut_out(0, data, contextOut);
        return;
    } else {
        // increment the stored sequence number
        this->sequenceNumber.store(sequenceNumber + 1);
    }

    Fw::Buffer computedHmac = this->computeHMAC(securityHeader, 6, data.getData(), data.getSize(), key_authn);
    const U8* computedHmacData = computedHmac.getData();
    const FwSizeType computedHmacLength = computedHmac.getSize();
    if (computedHmacData == nullptr || computedHmacLength < 8) {
        this->log_WARNING_HI_InvalidHash(context.get_apid(), spi, sequenceNumber);
        contextOut.set_authenticated(0);
        this->dataOut_out(0, data, contextOut);
        return;
    }

    bool hmacValid = this->compareHMAC(securityTrailer, computedHmacData, computedHmacLength);
    if (!hmacValid) {
        this->log_WARNING_HI_InvalidHash(context.get_apid(), spi, sequenceNumber);
        contextOut.set_authenticated(0);
        this->dataOut_out(0, data, contextOut);
        return;
    }

    this->log_ACTIVITY_HI_ValidHash(context.get_apid(), spi, sequenceNumber);
    contextOut.set_authenticated(1);

    this->dataOut_out(0, data, contextOut);
}

Authenticate::AuthenticationConfig Authenticate ::lookupAuthenticationConfig(U32 spi) {
    AuthenticationConfig config{};
    config.type = DEFAULT_AUTHENTICATION_TYPE;
    config.key = DEFAULT_AUTHENTICATION_KEY;

    // convert the spi from a decimal number to a hex string
    std::stringstream ss;
    ss << std::hex << spi;
    std::string spiHex = ss.str();
    // pad the spi hex string with 0s to make it 4 characters long
    spiHex = std::string(4 - spiHex.length(), '0') + spiHex;
    // add a space at the end and before the first character an enter
    spiHex = "_" + spiHex + " ";

    Os::File spiDictFile;
    Os::File::Status openStatus = spiDictFile.open(SPI_DICT_PATH, Os::File::OPEN_READ);
    if (openStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_FileOpenError(openStatus);
        return config;
    }

    FwSizeType fileSize = 0;
    Os::File::Status sizeStatus = spiDictFile.size(fileSize);
    if (sizeStatus != Os::File::OP_OK || fileSize == 0) {
        spiDictFile.close();
        this->log_WARNING_HI_FileOpenError(sizeStatus);
        return config;
    }

    std::string fileContents;
    fileContents.resize(static_cast<size_t>(fileSize), '\0');
    FwSizeType bytesToRead = fileSize;
    Os::File::Status readStatus =
        spiDictFile.read(reinterpret_cast<U8*>(&fileContents[0]), bytesToRead, Os::File::WaitType::WAIT);
    spiDictFile.close();
    if (readStatus != Os::File::OP_OK || bytesToRead != fileSize) {
        this->log_WARNING_HI_FileOpenError(readStatus);
        return config;
    }
    // find the line that contains the spi hex string
    size_t pos = fileContents.find(spiHex);
    if (pos != std::string::npos) {
        // get everything in the line after the spi hex string
        std::string line = fileContents.substr(pos);
        // get all the words in the line
        std::vector<std::string> words;
        std::istringstream iss(line);
        std::string word;
        while (iss >> word) {
            words.push_back(word);
        }
        this->log_ACTIVITY_LO_FoundSPIKey(true);
        config.type = words[1];
        config.key = words[2];
    } else {
        this->log_ACTIVITY_LO_FoundSPIKey(false);
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
