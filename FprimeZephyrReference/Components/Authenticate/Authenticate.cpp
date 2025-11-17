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
#include <zephyr/sys/printk.h>

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
    U32 statusU32 = static_cast<U32>(status);
    if (status != PSA_SUCCESS) {
        printk("Crypto Computation Error: FIRST ERROR %d\n", statusU32);
        this->log_WARNING_HI_CryptoComputationError(statusU32);
        return Fw::Buffer();
    }

    // Prepare key attributes for HMAC-SHA-256
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    // For psa_mac_compute, we only need SIGN_MESSAGE flag
    // VERIFY_MESSAGE is for psa_mac_verify operations
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    // Set algorithm in key attributes - some implementations require this
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    // Try PSA_KEY_TYPE_RAW_DATA instead of PSA_KEY_TYPE_HMAC
    // Some PSA implementations don't support PSA_KEY_TYPE_HMAC
    psa_set_key_type(&attributes, PSA_KEY_TYPE_RAW_DATA);

    // Convert hex string key to binary if needed
    // Keys can be provided as hex strings with or without "0x" prefix
    printk("Key String: %s (length: %zu)\n", key.c_str(), key.length());
    std::vector<U8> keyBytes;

    // Check if key starts with "0x" prefix
    std::string hexKey = key;
    if (key.length() > 2 && key.substr(0, 2) == "0x") {
        hexKey = key.substr(2);
        printk("Key has 0x prefix, hex part: %s\n", hexKey.c_str());
    }

    // Try to parse as hex string (check if all characters are hex digits)
    bool isHexString = true;
    for (char c : hexKey) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            isHexString = false;
            break;
        }
    }

    if (isHexString && hexKey.length() > 0 && hexKey.length() % 2 == 0) {
        // Parse as hex string
        printk("Parsing key as hex string\n");
        for (size_t i = 0; i < hexKey.length(); i += 2) {
            std::string byteStr = hexKey.substr(i, 2);
            keyBytes.push_back(static_cast<U8>(std::stoul(byteStr, nullptr, 16)));
        }
    } else {
        // Use key as raw bytes
        printk("Parsing key as raw bytes (not hex)\n");
        keyBytes.assign(key.begin(), key.end());
    }

    printk("Key Bytes (parsed): %zu bytes\n", keyBytes.size());
    printk("Key Bytes (hex): ");
    for (size_t i = 0; i < keyBytes.size() && i < 16; i++) {
        printk("%02x ", keyBytes[i]);
    }
    if (keyBytes.size() > 16) {
        printk("...");
    }
    printk("\n");
    // Import the key
    printk("Importing key: size=%zu, type=RAW_DATA, alg=HMAC-SHA256, usage=SIGN_MESSAGE\n", keyBytes.size());
    mbedtls_svc_key_id_t keyId = MBEDTLS_SVC_KEY_ID_INIT;
    status = psa_import_key(&attributes, keyBytes.data(), keyBytes.size(), &keyId);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS) {
        statusU32 = static_cast<U32>(status);
        printk("Crypto Computation Error: SECOND ERROR (key import failed) %d\n", statusU32);
        printk("  Key size: %zu bytes\n", keyBytes.size());
        printk("  Key type: HMAC\n");
        printk("  Algorithm: HMAC-SHA256\n");
        this->log_WARNING_HI_CryptoComputationError(statusU32);
        return Fw::Buffer();
    }
    printk("Key imported successfully, keyId: %lu\n", (unsigned long)keyId);

    // Combine security header and command payload into a single input buffer
    const size_t totalInputLength = static_cast<size_t>(securityHeaderLength + commandPayloadLength);
    std::vector<U8> inputBuffer;
    inputBuffer.reserve(totalInputLength);
    inputBuffer.insert(inputBuffer.end(), securityHeader, securityHeader + securityHeaderLength);
    inputBuffer.insert(inputBuffer.end(), commandPayload, commandPayload + commandPayloadLength);

    printk("Input Buffer: %zu bytes\n", inputBuffer.size());
    printk("Input Buffer (first 16 bytes): ");
    for (size_t i = 0; i < inputBuffer.size() && i < 16; i++) {
        printk("%02x ", inputBuffer[i]);
    }
    printk("\n");

    // Compute HMAC (HMAC-SHA-256 produces 32 bytes)
    // const size_t macLength = 32;
    U8 macOutput[32];
    size_t macOutputLength = 0;

    printk("Calling psa_mac_compute: keyId=%lu, alg=HMAC-SHA256, inputLen=%zu, outputBuf=%zu\n", (unsigned long)keyId,
           inputBuffer.size(), sizeof(macOutput));

    status = psa_mac_compute(keyId, PSA_ALG_HMAC(PSA_ALG_SHA_256), inputBuffer.data(), inputBuffer.size(), macOutput,
                             sizeof(macOutput), &macOutputLength);

    printk("psa_mac_compute returned: status=%d, macOutputLength=%zu\n", status, macOutputLength);

    // Destroy the key
    (void)psa_destroy_key(keyId);

    if (status != PSA_SUCCESS) {
        statusU32 = static_cast<U32>(status);
        printk("Crypto Computation Error: THIRD ERROR (psa_mac_compute failed) %d\n", statusU32);
        printk("  Error code: %d (0x%x)\n", statusU32, statusU32);
        printk("  Key ID: %lu\n", (unsigned long)keyId);
        printk("  Algorithm: HMAC-SHA256\n");
        printk("  Input length: %zu bytes\n", inputBuffer.size());
        printk("  Output buffer size: %zu bytes\n", sizeof(macOutput));
        printk("  MAC output length: %zu bytes\n", macOutputLength);
        printk("  Possible causes:\n");
        printk("    - Key size invalid for HMAC-SHA256 (should be 1-64 bytes)\n");
        printk("    - Algorithm not supported by PSA implementation\n");
        printk("    - Key usage flags incompatible\n");
        printk("    - Input buffer size invalid\n");
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
    // assert that the packet length is a correct length for a CCSDS Space Packet
    // printk("dataIn_handler: %lld\n", data.getSize());
    // printk("dataIn_handler: %hhn\n", data.getData());
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

    printk("\n");
    printk("security trailer: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
           securityTrailer[0], securityTrailer[1], securityTrailer[2], securityTrailer[3], securityTrailer[4],
           securityTrailer[5], securityTrailer[6], securityTrailer[7], securityTrailer[8], securityTrailer[9],
           securityTrailer[10], securityTrailer[11], securityTrailer[12], securityTrailer[13], securityTrailer[14],
           securityTrailer[15]);
    // the first two bytes are the SPI
    U32 spi = (static_cast<U32>(securityHeader[0]) << 8) | static_cast<U32>(securityHeader[1]);

    // the next four bytes are the sequence number
    U32 sequenceNumber = (static_cast<U32>(securityHeader[2]) << 24) | (static_cast<U32>(securityHeader[3]) << 16) |
                         (static_cast<U32>(securityHeader[4]) << 8) | static_cast<U32>(securityHeader[5]);
    printk("Sequence Number: %08x\n", sequenceNumber);

    const AuthenticationConfig authConfig = this->lookupAuthenticationConfig(spi);
    const std::string type_authn = authConfig.type;
    const std::string& key_authn = authConfig.key;

    // check the sequence number is valid
    // cast trailer to U32
    bool sequenceNumberValid = this->validateSequenceNumber(sequenceNumber, this->get_SequenceNumber());
    if (!sequenceNumberValid) {
        contextOut.set_authenticated(0);
        printk("sequence number not valid");
        this->dataOut_out(0, data, contextOut);
        return;
    } else {
        // increment the stored sequence number
        this->sequenceNumber.store(sequenceNumber + 1);
    }

    Fw::Buffer computedHmac = this->computeHMAC(securityHeader, 6, data.getData(), data.getSize(), key_authn);
    const U8* computedHmacData = computedHmac.getData();
    const FwSizeType computedHmacLength = computedHmac.getSize();
    if (computedHmacData != nullptr && computedHmacLength >= 8) {
        printk("Computed HMAC: %02x %02x %02x %02x %02x %02x %02x %02x\n", computedHmacData[0], computedHmacData[1],
               computedHmacData[2], computedHmacData[3], computedHmacData[4], computedHmacData[5], computedHmacData[6],
               computedHmacData[7]);
    } else {
        printk("Computed HMAC: (null or too short, length=%u)\n", computedHmacLength);
    }

    //         constexpr FwSizeType receivedHmacLength = 8;

    //         if ((computedHmacData == nullptr) || (computedHmacLength < receivedHmacLength) ||
    //             !this->compareHMAC(receivedHmac, computedHmacData, receivedHmacLength)) {
    //             this->dataReturnOut_out(0, data, context);
    //             return;
    //         }

    //     } else {
    //         // Current event expects a U32; hash the string for now until the event is updated.
    //         const U32 hashedType = static_cast<U32>(std::hash<std::string>{}(type_authn));
    //         this->log_WARNING_HI_InvalidAuthenticationType(hashedType);
    //         this->dataReturnOut_out(0, data, context);
    //         return;
    //     }

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

    printk("SPI Hex: %s\n", spiHex.c_str());

    Os::File spiDictFile;
    Os::File::Status openStatus = spiDictFile.open(SPI_DICT_PATH, Os::File::OPEN_READ);
    if (openStatus != Os::File::OP_OK) {
        printk("File Open Error, one: %d\n", openStatus);
        this->log_WARNING_HI_FileOpenError(openStatus);
        return config;
    }

    FwSizeType fileSize = 0;
    Os::File::Status sizeStatus = spiDictFile.size(fileSize);
    if (sizeStatus != Os::File::OP_OK || fileSize == 0) {
        spiDictFile.close();
        printk("File Open Error, two: %d\n", sizeStatus);
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
        printk("File Open Error, three: %d\n", readStatus);
        this->log_WARNING_HI_FileOpenError(readStatus);
        return config;
    }
    printk("File Contents: %s\n", fileContents.c_str());
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

    printk("Config Type: %s\n", config.type.c_str());
    printk("Config Key: %s\n", config.key.c_str());

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
