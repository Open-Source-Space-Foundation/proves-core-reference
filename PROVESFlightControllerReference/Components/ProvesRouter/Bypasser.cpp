// ======================================================================
// \title  Bypasser.cpp
// \brief  cpp file for packet policy validation helper functions
// ======================================================================

#include "Bypasser.hpp"

namespace Components {
namespace PacketBypasser {
namespace {

//! Struct to hold the result of parsing a field
template <typename T>
struct FieldParseResult {
    bool success;  //!< Whether the parsing was successful
    T value;       //!< The parsed value
};

//! Parse the OpCode field from the packet buffer
FieldParseResult<uint32_t> parseOpCode(const uint8_t* buffer, const size_t size) {
    // OpCode location and size in bytes
    constexpr const size_t kOpCodeOffset = 2;
    constexpr const size_t kOpCodeSize = 4;

    // Validate buffer size
    if (!buffer || size < kOpCodeOffset + kOpCodeSize) {
        return {false, 0};
    }

    // Extract opcode
    const uint8_t* opCodePtr = buffer + kOpCodeOffset;
    const uint32_t opCode = (static_cast<uint32_t>(opCodePtr[0]) << 24) | (static_cast<uint32_t>(opCodePtr[1]) << 16) |
                            (static_cast<uint32_t>(opCodePtr[2]) << 8) | static_cast<uint32_t>(opCodePtr[3]);

    return {true, opCode};
}

//! OpCodes that are allowed to bypass authentication
//! To determine the OpCode value for a command, search for the command in
//! build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json
//! then convert the opcode field value from decimal to hexadecimal
static constexpr uint32_t kBypassOpCodes[] = {
    0x01000000,  //!< CdhCore.cmdDisp.CMD_NO_OP
    0x2100B000,  //!< ComCcsdsUart.tcSecurityDeframer.GET_SEQ_NUM
    0x2200B000,  //!< ComCcsdsLora.tcSecurityDeframer.GET_SEQ_NUM
    0x2300B000,  //!< ComCcsdsSband.tcSecurityDeframer.GET_SEQ_NUM
    0x2100B002,  //!< ComCcsdsUart.tcSecurityDeframer.PROVISION_KEY
    0x2200B002,  //!< ComCcsdsLora.tcSecurityDeframer.PROVISION_KEY
    0x2300B002,  //!< ComCcsdsSband.tcSecurityDeframer.PROVISION_KEY
    0x10065000,  //!< ReferenceDeployment.amateurRadio.TELL_JOKE
};

//! Check if the OpCode is in the bypass list
bool opCodeBypassAllowed(uint32_t opCode) {
    // Check if the OpCode is in the bypass list
    for (size_t i = 0; i < (sizeof(kBypassOpCodes) / sizeof(kBypassOpCodes[0])); i++) {
        if (opCode == kBypassOpCodes[i]) {
            return true;
        }
    }

    return false;
}

}  // namespace

bool bypassPacket(const uint8_t* buffer, const size_t size) {
    // Parse the OpCode from the packet
    const FieldParseResult<uint32_t> parseResult = parseOpCode(buffer, size);
    if (!parseResult.success) {
        return false;
    }

    // Check if the packet can bypass authentication based on its OpCode
    if (opCodeBypassAllowed(parseResult.value)) {
        return true;
    }

    return false;
}

}  // namespace PacketBypasser
}  // namespace Components
