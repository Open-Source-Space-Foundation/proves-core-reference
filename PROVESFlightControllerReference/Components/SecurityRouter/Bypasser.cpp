// ======================================================================
// \title  Bypasser.cpp
// \brief  cpp file for packet policy validation helper functions
// ======================================================================

#include "Const.hpp"
#include "Bypasser.hpp"
#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Const.hpp"

namespace Components {
namespace {

constexpr const size_t kMinPacketSize =
    Ccsds355_0_B_2::kTCSecurityHeaderSize + SpacePacket::kHeaderSize + SpacePacket::kOpCodeOffset + SpacePacket::kOpCodeSize;  //!< Minimum packet size

//! Struct to hold the result of parsing a field
template <typename T>
struct FieldParseResult {
    bool success;  //!< Whether the parsing was successful
    T value;       //!< The parsed value
};

//! Parse the OpCode field from the packet buffer
FieldParseResult<uint32_t> parseOpCode(const uint8_t* buffer, const size_t size) {
    // Validate buffer size
    if (!buffer || size < kMinPacketSize) {
        return {false, 0};
    }

    // Extract opcode
    const uint8_t* opCodePtr = buffer + Ccsds355_0_B_2::kTCSecurityHeaderSize +
                               SpacePacket::kHeaderSize + SpacePacket::kOpCodeOffset;
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
    0x2100B000,  //!< ComCcsdsUart.SecurityRouter.GET_SEQ_NUM
    0x2200B000,  //!< ComCcsdsLora.SecurityRouter.GET_SEQ_NUM
    0x2300B000,  //!< ComCcsdsSBand.SecurityRouter.GET_SEQ_NUM
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

PacketBypasser::Status bypassPacket(const uint8_t* buffer, const size_t size) {
    // Parse the OpCode from the packet
    const FieldParseResult<uint32_t> parseResult = parseOpCode(buffer, size);
    if (!parseResult.success) {
        return PacketBypasser::Status::OpCodeParseError;
    }

    // Check if the packet can bypass authentication based on its OpCode
    if (opCodeBypassAllowed(parseResult.value)) {
        return PacketBypasser::Status::BypassAllowed;
    }

    return PacketBypasser::Status::RequiresAuthentication;
}

}  // namespace Components
