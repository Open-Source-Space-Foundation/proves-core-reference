// ======================================================================
// \title  AuthenticationRouter.cpp
// \author Ines (based on thomas-bc's FprimeRouter.cpp)
// \brief  cpp file for AuthenticationRouter component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/AuthenticationRouter/AuthenticationRouter.hpp"

#include <Fw/Log/LogString.hpp>
#include <Fw/Types/String.hpp>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "Fw/Com/ComPacket.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Os/File.hpp"
#include "config/ApidEnumAc.hpp"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

constexpr const U8 OP_CODE_LENGTH = 4;  // F Prime opcodes are 32-bit (4 bytes)
constexpr const U8 OP_CODE_START = 2;   // Opcode starts at byte offset 2 in the packet buffer

// List of opcodes (as hex strings) that bypass authentication
// Format: 8 hex characters (4 bytes = 32-bit opcode)
// Example: "00000001" for opcode 0x00000001
static constexpr const char* kBypassOpCodes[] = {
    // Add opcodes here as hex strings (uppercase, no spaces, no 0x prefix)
    "01000000",  // no op
    nullptr      // Sentinel to mark end of list
};

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AuthenticationRouter ::AuthenticationRouter(const char* const compName) : AuthenticationRouterComponentBase(compName) {}
AuthenticationRouter ::~AuthenticationRouter() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

// Helper function to convert binary data to hex string for debugging
static void printHexData(const U8* data, size_t length, const char* label) {
    if (data == nullptr || length == 0) {
        printk("%s: (null or empty)\n", label);
        return;
    }
    printk("%s: ", label);
    for (size_t i = 0; i < length; i++) {
        printk("%02X ", data[i]);
    }
    printk("\n");
}

bool AuthenticationRouter::BypassesAuthentification(Fw::Buffer& packetBuffer) {
    // Check bounds before extracting
    if (packetBuffer.getSize() < (OP_CODE_START + OP_CODE_LENGTH)) {
        return false;
    }

    // Extract opcode bytes
    U8 opCodeBytes[OP_CODE_LENGTH];
    std::memcpy(opCodeBytes, packetBuffer.getData() + OP_CODE_START, OP_CODE_LENGTH);

    // TO DO: See if I can save the opcode in hex form instead of converting it every time
    constexpr size_t kHexStrSize = OP_CODE_LENGTH * 2 + 1;
    char opCodeHex[kHexStrSize];
    for (size_t i = 0; i < OP_CODE_LENGTH; i++) {
        const size_t remainingSize = kHexStrSize - (i * 2);
        std::snprintf(opCodeHex + (i * 2), remainingSize, "%02X", static_cast<unsigned>(opCodeBytes[i]));
    }
    // Null-terminate string
    opCodeHex[OP_CODE_LENGTH * 2] = '\0';

    // Check if opcode matches any in the bypass list
    for (size_t i = 0; kBypassOpCodes[i] != nullptr; i++) {
        if (std::strcmp(opCodeHex, kBypassOpCodes[i]) == 0) {
            printk("[DEBUG] BypassesAuthentification: Found matching opcode: %s\n", opCodeHex);
            return true;
        }
    }

    return false;
}

void AuthenticationRouter ::dataIn_handler(FwIndexType portNum,
                                           Fw::Buffer& packetBuffer,
                                           const ComCfg::FrameContext& context) {
    printk("context %d", context.get_authenticated());

    // Check if the OpCodes are in the OpCode list
    // TODO
    bool bypasses = this->BypassesAuthentification(packetBuffer);

    // the packet was not authenticated
    if (context.get_authenticated() == 0 && !bypasses) {
        // emit reject packet event
        this->log_ACTIVITY_LO_PassedRouter(false);
        // Return ownership of the incoming packetBuffer
        // for now we want to run all the commands!! Later un comment this
        this->dataReturnOut_out(0, packetBuffer, context);
        return;
    }

    if (bypasses) {
        // emit bypass event
        this->log_ACTIVITY_LO_BypassedAuthentification();
    }

    Fw::SerializeStatus status;
    Fw::ComPacketType packetType = context.get_apid();
    // Route based on received APID (packet type)
    switch (packetType) {
        // Handle a command packet
        case Fw::ComPacketType::FW_PACKET_COMMAND: {
            // Allocate a com buffer on the stack
            Fw::ComBuffer com;
            // Copy the contents of the packet buffer into the com buffer
            status = com.setBuff(packetBuffer.getData(), packetBuffer.getSize());
            if (status == Fw::FW_SERIALIZE_OK) {
                // Send the com buffer - critical functionality so it is considered an error not to
                // have the port connected. This is why we don't check isConnected() before sending.
                this->commandOut_out(0, com, 0);
            } else {
                this->log_WARNING_HI_SerializationError(status);
            }
            break;
        }
        // Handle a file packet
        case Fw::ComPacketType::FW_PACKET_FILE: {
            // If the file uplink output port is connected, send the file packet. Otherwise take no action.
            if (this->isConnected_fileOut_OutputPort(0)) {
                // Copy buffer into a new allocated buffer. This lets us return the original buffer with dataReturnOut,
                // and FprimeRouter can handle the deallocation of the file buffer when it returns on fileBufferReturnIn
                Fw::Buffer packetBufferCopy = this->bufferAllocate_out(0, packetBuffer.getSize());
                // Confirm we got a valid buffer before using it
                if (packetBufferCopy.isValid()) {
                    auto copySerializer = packetBufferCopy.getSerializer();
                    status = copySerializer.serializeFrom(packetBuffer.getData(), packetBuffer.getSize(),
                                                          Fw::Serialization::OMIT_LENGTH);
                    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
                    // Send the copied buffer out. It will come back on fileBufferReturnIn once the receiver is done
                    // with it
                    this->fileOut_out(0, packetBufferCopy);
                } else {
                    this->log_WARNING_HI_AllocationError(AuthenticationRouter_AllocationReason::FILE_UPLINK);
                }
            }
            break;
        }
        default: {
            // Packet type is not known to the F Prime protocol. If the unknownDataOut port is
            // connected, forward packet and context for further processing
            if (this->isConnected_unknownDataOut_OutputPort(0)) {
                // Copy buffer into a new allocated buffer. This lets us return the original buffer with dataReturnOut,
                // and FprimeRouter can handle the deallocation of the unknown buffer when it returns on bufferReturnIn
                Fw::Buffer packetBufferCopy = this->bufferAllocate_out(0, packetBuffer.getSize());
                // Confirm we got a valid buffer before using it
                if (packetBufferCopy.isValid()) {
                    auto copySerializer = packetBufferCopy.getSerializer();
                    status = copySerializer.serializeFrom(packetBuffer.getData(), packetBuffer.getSize(),
                                                          Fw::Serialization::OMIT_LENGTH);
                    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
                    // Send the copied buffer out. It will come back on fileBufferReturnIn once the receiver is done
                    // with it
                    this->unknownDataOut_out(0, packetBufferCopy, context);
                } else {
                    this->log_WARNING_HI_AllocationError(AuthenticationRouter_AllocationReason::USER_BUFFER);
                }
            }
            break;
        }
    }

    // Return ownership of the incoming packetBuffer
    this->dataReturnOut_out(0, packetBuffer, context);
}

void AuthenticationRouter ::cmdResponseIn_handler(FwIndexType portNum,
                                                  FwOpcodeType opcode,
                                                  U32 cmdSeq,
                                                  const Fw::CmdResponse& response) {
    // Nothing to do
}

void AuthenticationRouter ::fileBufferReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->bufferDeallocate_out(0, fwBuffer);
}

}  // namespace Svc
