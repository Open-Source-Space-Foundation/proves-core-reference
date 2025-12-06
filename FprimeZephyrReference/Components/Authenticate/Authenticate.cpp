// ======================================================================
// \title  Authenticate.cpp
// \author Ines
// \brief  cpp file for Authenticate component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Authenticate/Authenticate.hpp"

// Include configuration header to check if authentication is enabled
#include "AuthenticateCfg.hpp"

#if AUTHENTICATE_ENABLED
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

// Include generated header with default key (generated at build time)
#include "AuthDefaultKey.h"

// Hardcoded Dictionary of Authentication Types
constexpr const char DEFAULT_AUTHENTICATION_TYPE[] = "HMAC";
constexpr const char DEFAULT_AUTHENTICATION_KEY[] = AUTH_DEFAULT_KEY;
constexpr const char SPI_DICT_PATH[] = "//spi_dict.txt";
constexpr const char SEQUENCE_NUMBER_PATH[] = "//sequence_number.txt";
constexpr const char REJECTED_PACKETS_COUNT_PATH[] = "//rejected_packets_count.txt";
constexpr const char AUTHENTICATED_PACKETS_COUNT_PATH[] = "//authenticated_packets_count.txt";
constexpr const int SECURITY_HEADER_LENGTH = 6;
constexpr const int SECURITY_TRAILER_LENGTH = 16;
#endif

// TO DO: ADD TO THE DOWNLINK PATH FOR LORA AND S BAND AS WELL
// TO DO GIVE THE CHOICE FOR NOT JUST HMAC BUT ALSO OTHER AUTHENTICATION TYPES

namespace Components {
// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Authenticate ::Authenticate(const char* const compName) : AuthenticateComponentBase(compName), sequenceNumber(0) {
#if AUTHENTICATE_ENABLED
    U32 fileSequenceNumber = this->initializeFiles(SEQUENCE_NUMBER_PATH);
    this->sequenceNumber.store(fileSequenceNumber);
    this->tlmWrite_CurrentSequenceNumber(fileSequenceNumber);

    U32 fileRejectedPacketsCount = this->initializeFiles(REJECTED_PACKETS_COUNT_PATH);
    this->m_rejectedPacketsCount.store(fileRejectedPacketsCount);
    this->tlmWrite_RejectedPacketsCount(fileRejectedPacketsCount);

    U32 fileAuthenticatedPacketsCount = this->initializeFiles(AUTHENTICATED_PACKETS_COUNT_PATH);
    this->m_authenticatedPacketsCount.store(fileAuthenticatedPacketsCount);
    this->tlmWrite_AuthenticatedPacketsCount(fileAuthenticatedPacketsCount);
#else
    // Authentication disabled - initialize with defaults
    this->sequenceNumber.store(0);
    this->tlmWrite_CurrentSequenceNumber(0);
    this->m_rejectedPacketsCount.store(0);
    this->tlmWrite_RejectedPacketsCount(0);
    this->m_authenticatedPacketsCount.store(0);
    this->tlmWrite_AuthenticatedPacketsCount(0);
#endif
}

U32 Authenticate::initializeFiles(const char* filePath) {
    U32 count = 0;
    bool loadedFromFile = false;

    // Use a local file object instead of a member variable
    Os::File file;

    // Try to open and read from the file
    Os::File::Status openStatus = file.open(filePath, Os::File::OPEN_READ);
    if (openStatus == Os::File::OP_OK) {
        FwSizeType size = static_cast<FwSizeType>(sizeof(count));
        FwSizeType expectedSize = size;
        Os::File::Status readStatus = file.read(reinterpret_cast<U8*>(&count), size, Os::File::WaitType::WAIT);
        file.close();
        loadedFromFile = (readStatus == Os::File::OP_OK) && (size == expectedSize);
    }

    // If file doesn't exist or read failed, create it with value 0
    if (!loadedFromFile) {
        count = 0;
        Os::File::Status createStatus =
            file.open(filePath, Os::File::OPEN_CREATE, Os::File::OverwriteType::NO_OVERWRITE);
        if (createStatus == Os::File::OP_OK) {
            const U8* buffer = reinterpret_cast<const U8*>(&count);
            FwSizeType size = static_cast<FwSizeType>(sizeof(count));
            (void)file.write(buffer, size, Os::File::WaitType::WAIT);
            file.close();
        }
    }

    return count;
}

void Authenticate::persistToFile(const char* filePath, U32 value) {
    Os::File file;
    Os::File::Status openStatus = file.open(filePath, Os::File::OPEN_CREATE, Os::File::OverwriteType::OVERWRITE);
    if (openStatus == Os::File::OP_OK) {
        const U8* buffer = reinterpret_cast<const U8*>(&value);
        FwSizeType size = static_cast<FwSizeType>(sizeof(value));
        (void)file.write(buffer, size, Os::File::WaitType::WAIT);
        file.close();
    }
}

#if AUTHENTICATE_ENABLED
void Authenticate::rejectPacket(Fw::Buffer& data, ComCfg::FrameContext& contextOut) {
    U32 newCount = this->m_rejectedPacketsCount.load() + 1;
    this->m_rejectedPacketsCount.store(newCount);
    this->tlmWrite_RejectedPacketsCount(newCount);
    this->persistToFile(REJECTED_PACKETS_COUNT_PATH, newCount);
    contextOut.set_authenticated(0);
    this->dataOut_out(0, data, contextOut);
}
#endif

Authenticate ::~Authenticate() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

#if AUTHENTICATE_ENABLED
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

    printk("[DEBUG] computeHMAC: Input key string='%s', length=%zu\n", key.c_str(), key.length());

    // Strip "0x" or "0X" prefix if present
    if (hexKey.length() >= 2 && (hexKey.substr(0, 2) == "0x" || hexKey.substr(0, 2) == "0X")) {
        hexKey = hexKey.substr(2);
        printk("[DEBUG] computeHMAC: Stripped 0x prefix, remaining='%s'\n", hexKey.c_str());
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
        printk("[DEBUG] computeHMAC: Parsing key as hex string\n");
        for (size_t i = 0; i < hexKey.length(); i += 2) {
            std::string byteStr = hexKey.substr(i, 2);
            keyBytes.push_back(static_cast<U8>(std::stoul(byteStr, nullptr, 16)));
        }
    } else {
        printk("[DEBUG] computeHMAC: Parsing key as raw bytes (not hex string), isHexString=%d, hexKey.length()=%zu\n",
               isHexString ? 1 : 0, hexKey.length());
        keyBytes.assign(key.begin(), key.end());
    }

    printk("[DEBUG] computeHMAC: Parsed key bytes (length=%zu): ", keyBytes.size());
    for (size_t i = 0; i < keyBytes.size() && i < 16; i++) {
        printk("%02X ", keyBytes[i]);
    }
    printk("\n");

    // Check the length of the key bytes
    if (keyBytes.size() != 16) {
        printk("[DEBUG] computeHMAC: ERROR - Key length is %zu, expected 16 bytes\n", keyBytes.size());
        this->log_WARNING_HI_InvalidSPI(-1);
        return Fw::Buffer();
    }

    // Combine security header and command payload into a single input buffer
    const size_t totalInputLength = static_cast<size_t>(securityHeaderLength + commandPayloadLength);
    printk(
        "[DEBUG] computeHMAC: Input data - securityHeaderLength=%llu, commandPayloadLength=%llu, "
        "totalInputLength=%zu\n",
        static_cast<unsigned long long>(securityHeaderLength), static_cast<unsigned long long>(commandPayloadLength),
        totalInputLength);

    printk("[DEBUG] computeHMAC: Security header bytes: ");
    for (FwSizeType i = 0; i < securityHeaderLength && i < 6; i++) {
        printk("%02X ", securityHeader[i]);
    }
    printk("\n");

    printk("[DEBUG] computeHMAC: Command payload (first 16 bytes): ");
    for (FwSizeType i = 0; i < commandPayloadLength && i < 16; i++) {
        printk("%02X ", commandPayload[i]);
    }
    printk("\n");

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
        printk("[DEBUG] computeHMAC: ERROR - Failed to allocate memory for HMAC result\n");
        return Fw::Buffer();
    }
    std::memcpy(hmacData, macOutput, hmacOutputLength);

    printk("[DEBUG] computeHMAC: Computed HMAC result (16 bytes): ");
    for (size_t i = 0; i < hmacOutputLength; i++) {
        printk("%02X ", hmacData[i]);
    }
    printk("\n");

    // Create Fw::Buffer with the HMAC data (context 0 for now)
    return Fw::Buffer(hmacData, static_cast<FwSizeType>(hmacOutputLength), 0);
}
#endif

bool Authenticate::validateSequenceNumber(U32 received, U32 expected) {
    // validate the sequence number by checking if it is within the window of the expected sequence number
    Fw::ParamValid valid;
    U32 window = paramGet_SEQ_NUM_WINDOW(valid);
    /*
     * Compute the difference between received and expected sequence numbers using unsigned
     * 32-bit arithmetic. This handles wraparound correctly due to the well-defined behavior
     * of unsigned integer overflow in C++. For example, if expected=0xFFFFFFFE and received=1,
     * then (received - expected) == 3 (modulo 2^32). This is a standard technique for
     * sequence number window validation (see RFC 1982: Serial Number Arithmetic).
     */
    const U32 delta = received - expected;
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

#if AUTHENTICATE_ENABLED
bool Authenticate::validateHMAC(const U8* securityHeader,
                                FwSizeType securityHeaderLength,
                                const U8* data,
                                FwSizeType dataLength,
                                const std::string& key,
                                const U8* securityTrailer) {
    printk("[DEBUG] validateHMAC: Computing HMAC with key='%s', securityHeaderLength=%llu, dataLength=%llu\n",
           key.c_str(), static_cast<unsigned long long>(securityHeaderLength),
           static_cast<unsigned long long>(dataLength));

    // Compute HMAC (allocates memory)
    Fw::Buffer computedHmac = this->computeHMAC(securityHeader, securityHeaderLength, data, dataLength, key);
    const U8* computedHmacData = computedHmac.getData();
    const FwSizeType computedHmacLength = computedHmac.getSize();

    // Store non-const pointer for deletion (getData() returns const, but we allocated with new[])
    // This pointer is scoped to this function only - cannot be accidentally used after return
    U8* hmacDataToDelete = const_cast<U8*>(computedHmacData);

    if (computedHmacData == nullptr || computedHmacLength < 8) {
        printk("[DEBUG] validateHMAC: ERROR - computedHmacData is null or too short (length=%llu)\n",
               static_cast<unsigned long long>(computedHmacLength));
        // Clean up memory before returning (delete[] nullptr is safe)
        delete[] hmacDataToDelete;
        return false;
    }

    printk("[DEBUG] validateHMAC: Computed HMAC (first 16 bytes): ");
    for (FwSizeType i = 0; i < (computedHmacLength < 16 ? computedHmacLength : 16); i++) {
        printk("%02X ", computedHmacData[i]);
    }
    printk("\n");

    printk("[DEBUG] validateHMAC: Expected HMAC from securityTrailer (16 bytes): ");
    for (FwSizeType i = 0; i < 16; i++) {
        printk("%02X ", securityTrailer[i]);
    }
    printk("\n");

    // Compare computed HMAC with expected HMAC from security trailer
    bool hmacValid = this->compareHMAC(computedHmacData, securityTrailer, computedHmacLength);

    if (!hmacValid) {
        printk("[DEBUG] validateHMAC: HMAC comparison FAILED - computed and expected do not match\n");
    } else {
        printk("[DEBUG] validateHMAC: HMAC comparison PASSED\n");
    }

    delete[] hmacDataToDelete;

    return hmacValid;
}
#endif

#if AUTHENTICATE_ENABLED
bool Authenticate::validateHeader(Fw::Buffer& data,
                                  ComCfg::FrameContext& contextOut,
                                  U8* securityHeader,
                                  U8* securityTrailer,
                                  U32& spi,
                                  U32& sequenceNumber) {
    // 34 = 12 (data) + 6 (security header) + 16 (security trailer)
    // some packets will be missed here, because we don't have a clear way to tell if the packet is long because its
    // authenticating or because its not a valid packet.
    // later we will change the integration tests to all run on plugins, but for now, only the 12 bytes will be
    // authenticated.

    // Take the first 6 bytes as the security header
    std::memcpy(securityHeader, data.getData(), SECURITY_HEADER_LENGTH);
    // increment the pointer to the data to point to the rest of the packet
    data.setData(data.getData() + SECURITY_HEADER_LENGTH);
    data.setSize(data.getSize() - SECURITY_HEADER_LENGTH);

    // now we get the footer (last 16 bytes)
    std::memcpy(securityTrailer, data.getData() + data.getSize() - SECURITY_TRAILER_LENGTH, SECURITY_TRAILER_LENGTH);
    // decrement the size of the data to remove the footer
    data.setSize(data.getSize() - SECURITY_TRAILER_LENGTH);

    // the first two bytes are the SPI
    spi = (static_cast<U32>(securityHeader[0]) << 8) | static_cast<U32>(securityHeader[1]);

    // the next four bytes are the sequence number
    sequenceNumber = (static_cast<U32>(securityHeader[2]) << 24) | (static_cast<U32>(securityHeader[3]) << 16) |
                     (static_cast<U32>(securityHeader[4]) << 8) | static_cast<U32>(securityHeader[5]);

    return true;
}
#endif

void Authenticate ::dataIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    ComCfg::FrameContext contextOut = context;

#if AUTHENTICATE_ENABLED
    // Authentication is enabled - perform full authentication checks
    // right now we pass everything that is not noop with the header and trailer, until we fix the integration tests to
    // all run on plugins.

    // Extract security header information
    U8 securityHeader[SECURITY_HEADER_LENGTH];
    U8 securityTrailer[SECURITY_TRAILER_LENGTH];
    U32 spi = 0;
    U32 sequenceNumber = 0;

    if (!this->validateHeader(data, contextOut, securityHeader, securityTrailer, spi, sequenceNumber)) {
        // Packet was rejected in validateHeader (already logged and output)
        this->rejectPacket(data, contextOut);
        return;
    }

    const AuthenticationConfig authConfig = this->lookupAuthenticationConfig(spi);
    const std::string type_authn = authConfig.type;
    const std::string& key_authn = authConfig.key;

    printk("[DEBUG] Authentication: APID=%u, SPI=0x%04X (%u), SeqNum=%u\n", contextOut.get_apid(), spi, spi,
           sequenceNumber);
    printk("[DEBUG] Authentication: Using auth type='%s', key='%s'\n", type_authn.c_str(), key_authn.c_str());

    // check the sequence number is valid
    U32 expectedSeqNum = this->get_SequenceNumber();
    printk("[DEBUG] Authentication: Expected seqNum=%u, received seqNum=%u\n", expectedSeqNum, sequenceNumber);
    bool sequenceNumberValid = this->validateSequenceNumber(sequenceNumber, expectedSeqNum);
    if (!sequenceNumberValid) {
        printk("[DEBUG] Authentication: Sequence number validation FAILED\n");
        this->rejectPacket(data, contextOut);
        return;
    }
    printk("[DEBUG] Authentication: Sequence number validation PASSED\n");

    // Validate HMAC - all memory management is handled inside validateHMAC()
    // The raw pointer hmacDataToDelete is scoped to validateHMAC() and cannot be
    // accidentally referenced after this call
    printk("[DEBUG] Authentication: Starting HMAC validation, dataLength=%llu\n",
           static_cast<unsigned long long>(data.getSize()));
    bool hmacValid = this->validateHMAC(securityHeader, 6, data.getData(), data.getSize(), key_authn, securityTrailer);

    if (!hmacValid) {
        printk("[DEBUG] Authentication: HMAC validation FAILED for APID=%u, SPI=0x%04X (%u), SeqNum=%u\n",
               contextOut.get_apid(), spi, spi, sequenceNumber);
        this->log_WARNING_HI_InvalidHash(contextOut.get_apid(), spi, sequenceNumber);
        this->rejectPacket(data, contextOut);
        return;
    }
    printk("[DEBUG] Authentication: HMAC validation PASSED\n");

    // increment the stored sequence number and persist it to the file system
    U32 newSequenceNumber = sequenceNumber + 1;
    this->sequenceNumber.store(newSequenceNumber);
    this->tlmWrite_CurrentSequenceNumber(newSequenceNumber);
    this->persistToFile(SEQUENCE_NUMBER_PATH, newSequenceNumber);

    this->log_ACTIVITY_HI_ValidHash(contextOut.get_apid(), spi, sequenceNumber);
    U32 newCount = this->m_authenticatedPacketsCount.load() + 1;
    this->m_authenticatedPacketsCount.store(newCount);
    this->tlmWrite_AuthenticatedPacketsCount(newCount);
    this->persistToFile(AUTHENTICATED_PACKETS_COUNT_PATH, newCount);
    contextOut.set_authenticated(1);
#else
    // Authentication is disabled - pass data through unchanged
    // Mark as authenticated (or leave context unchanged) and pass through
    contextOut.set_authenticated(
        1);  // Pass through as authenticated, or use context.authenticated if you want to preserve
#endif

    this->dataOut_out(0, data, contextOut);
}

#if AUTHENTICATE_ENABLED
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
        Fw::LogStringArg filenameArg(SPI_DICT_PATH);
        this->log_WARNING_HI_FileOpenError(openStatus, filenameArg);
        return config;
    }

    FwSizeType fileSize = 0;
    Os::File::Status sizeStatus = spiDictFile.size(fileSize);
    if (sizeStatus != Os::File::OP_OK || fileSize == 0) {
        spiDictFile.close();
        Fw::LogStringArg filenameArg(SPI_DICT_PATH);
        this->log_WARNING_HI_FileOpenError(sizeStatus, filenameArg);
        return config;
    }

    std::string fileContents;
    fileContents.resize(static_cast<size_t>(fileSize), '\0');
    FwSizeType bytesToRead = fileSize;
    Os::File::Status readStatus =
        spiDictFile.read(reinterpret_cast<U8*>(&fileContents[0]), bytesToRead, Os::File::WaitType::WAIT);
    spiDictFile.close();
    if (readStatus != Os::File::OP_OK || bytesToRead != fileSize) {
        Fw::LogStringArg filenameArg(SPI_DICT_PATH);
        this->log_WARNING_HI_FileOpenError(readStatus, filenameArg);
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
        if (words.size() < 3) {
            this->log_WARNING_HI_InvalidSPI(spi);
            return config;
        }
        config.type = words[1];
        config.key = words[2];
    } else {
        this->log_ACTIVITY_LO_FoundSPIKey(false);
    }

    return config;
}
#endif

void Authenticate ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& data, const ComCfg::FrameContext& context) {
    this->dataReturnOut_out(0, data, context);
}

#if AUTHENTICATE_ENABLED
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
        this->tlmWrite_CurrentSequenceNumber(fileSequenceNumber);
        return fileSequenceNumber;
    } else {
        fileSequenceNumber = this->sequenceNumber.load();
        return fileSequenceNumber;
    }
}
#endif

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

#if AUTHENTICATE_ENABLED
void Authenticate ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    U32 fileSequenceNumber = this->get_SequenceNumber();

    this->log_ACTIVITY_HI_EmitSequenceNumber(fileSequenceNumber);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}
#else
void Authenticate ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Authentication disabled - return current sequence number from memory
    U32 currentSeq = this->sequenceNumber.load();
    this->log_ACTIVITY_HI_EmitSequenceNumber(currentSeq);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}
#endif

#if AUTHENTICATE_ENABLED
void Authenticate ::GET_KEY_FROM_SPI_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 spi) {
    const AuthenticationConfig authConfig = this->lookupAuthenticationConfig(spi);
    Fw::LogStringArg keyArg(authConfig.key.c_str());
    Fw::LogStringArg typeArg(authConfig.type.c_str());
    this->log_ACTIVITY_HI_EmitSpiKey(keyArg, typeArg);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}
#else
void Authenticate ::GET_KEY_FROM_SPI_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 spi) {
    // Authentication disabled - command not supported
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::INVALID_OPCODE);
}
#endif

#if AUTHENTICATE_ENABLED
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
#else
void Authenticate ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
    // Authentication disabled - just store in memory, don't persist
    this->sequenceNumber.store(seq_num);
    this->log_ACTIVITY_HI_SetSequenceNumberSuccess(seq_num, false);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}
#endif

}  // namespace Components
