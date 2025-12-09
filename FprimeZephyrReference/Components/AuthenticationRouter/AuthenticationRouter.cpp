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

constexpr const char BYPASS_AUTHENTICATION_FILE[] = "//bypass_authentification_file.txt";
constexpr const U8 OP_CODE_LENGTH = 4;  // F Prime opcodes are 32-bit (4 bytes)
constexpr const U8 OP_CODE_START = 2;   // Opcode starts at byte offset 2 in the packet buffer

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

bool AuthenticationRouter ::BypassesAuthentification(Fw::Buffer& packetBuffer) {
    // Extract opCode from packet buffer
    printk("\n");
    printHexData(packetBuffer.getData(), packetBuffer.getSize(), "packetBuffer.getData()");
    printk("OP_CODE_START %d \n", OP_CODE_START);
    printk("OP_CODE_LENGTH %d \n", OP_CODE_LENGTH);

    // Check bounds before extracting
    if (packetBuffer.getSize() < (OP_CODE_START + OP_CODE_LENGTH)) {
        printk("ERROR: Buffer too small! Need %d bytes starting at offset %d, but buffer only has %llu bytes\n",
               OP_CODE_LENGTH, OP_CODE_START, static_cast<unsigned long long>(packetBuffer.getSize()));
        return false;
    }

    // Extract opcode bytes
    std::vector<U8> opCodeBytes(OP_CODE_LENGTH);
    std::memcpy(&opCodeBytes[0], packetBuffer.getData() + OP_CODE_START, OP_CODE_LENGTH);
    printHexData(opCodeBytes.data(), OP_CODE_LENGTH, "opCode");

    // Convert opcode bytes to hex string (uppercase, no spaces)
    Fw::String opCodeHex;
    char hexStr[OP_CODE_LENGTH * 2 + 1];
    for (size_t i = 0; i < OP_CODE_LENGTH; i++) {
        size_t remaining = sizeof(hexStr) - (i * 2);
        std::snprintf(hexStr + i * 2, remaining, "%02X", static_cast<unsigned>(opCodeBytes[i]));
    }
    hexStr[OP_CODE_LENGTH * 2] = '\0';
    opCodeHex = hexStr;
    printk("opCodeHex: %s\n", opCodeHex.toChar());

    // Open file
    Os::File bypassOpCodesFile;
    Os::File::Status openStatus = bypassOpCodesFile.open(BYPASS_AUTHENTICATION_FILE, Os::File::OPEN_READ);
    if (openStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_FileOpenError(openStatus);
        return false;
    }

    FwSizeType fileSize = 0;
    Os::File::Status sizeStatus = bypassOpCodesFile.size(fileSize);
    if (sizeStatus != Os::File::OP_OK || fileSize == 0) {
        bypassOpCodesFile.close();
        this->log_WARNING_HI_FileOpenError(sizeStatus);
        return false;
    }

    // Read file contents as text
    std::vector<U8> fileContentsBuffer(static_cast<size_t>(fileSize));
    FwSizeType bytesToRead = fileSize;
    Os::File::Status readStatus =
        bypassOpCodesFile.read(fileContentsBuffer.data(), bytesToRead, Os::File::WaitType::WAIT);
    bypassOpCodesFile.close();

    if (readStatus != Os::File::OP_OK || bytesToRead != fileSize) {
        this->log_WARNING_HI_FileOpenError(readStatus);
        return false;
    }

    // Null-terminate for string operations
    fileContentsBuffer.push_back('\0');
    const char* fileContents = reinterpret_cast<const char*>(fileContentsBuffer.data());
    printk("fileContents: %s\n", fileContents);

    // Parse file line by line and check if opcode hex string matches
    const char* opCodeHexStr = opCodeHex.toChar();
    const char* lineStart = fileContents;
    while (*lineStart != '\0') {
        // Find end of line
        const char* lineEnd = lineStart;
        while (*lineEnd != '\0' && *lineEnd != '\n' && *lineEnd != '\r') {
            lineEnd++;
        }

        // Extract line (without newline)
        FwSizeType lineLen = static_cast<FwSizeType>(lineEnd - lineStart);
        if (lineLen > 0) {
            Fw::String line;
            char* lineBuf = new char[lineLen + 1];
            std::strncpy(lineBuf, lineStart, static_cast<size_t>(lineLen));
            lineBuf[lineLen] = '\0';
            line = lineBuf;
            delete[] lineBuf;

            // Check if this line matches the opcode
            if (line == opCodeHex) {
                printk("Found matching opcode in bypass file: %s\n", line.toChar());
                return true;
            } else {
                printk("No matching opcode in bypass file: %s\n", line.toChar());
                printk("opCodeHex: %s\n", opCodeHexStr);
                printk("line: %s\n", line.toChar());
            }
        }

        // Move to next line
        lineStart = lineEnd;
        if (*lineStart == '\r') {
            lineStart++;
        }
        if (*lineStart == '\n') {
            lineStart++;
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
    if (context.get_authenticated() == 0 && bypasses == false) {
        // emit reject packet event
        this->log_ACTIVITY_LO_PassedRouter(false);
        // Return ownership of the incoming packetBuffer
        // for now we want to run all the commands!! Later un comment this
        this->dataReturnOut_out(0, packetBuffer, context);
        return;
    }

    if (bypasses == true) {
        // emit bypass event
        this->log_ACTIVITY_LO_BypassedAuthentification();
    }

    this->log_ACTIVITY_LO_PassedRouter(true);
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
                // and AuthenticationRouter can handle the deallocation of the file buffer when it returns on
                // fileBufferReturnIn
                Fw::Buffer packetBufferCopy = this->bufferAllocate_out(0, packetBuffer.getSize());
                auto copySerializer = packetBufferCopy.getSerializer();
                status = copySerializer.serializeFrom(packetBuffer.getData(), packetBuffer.getSize(),
                                                      Fw::Serialization::OMIT_LENGTH);
                FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
                // Send the copied buffer out. It will come back on fileBufferReturnIn once the receiver is done with it
                this->fileOut_out(0, packetBufferCopy);
            }
            break;
        }
        default: {
            // Packet type is not known to the F Prime protocol. If the unknownDataOut port is
            // connected, forward packet and context for further processing
            if (this->isConnected_unknownDataOut_OutputPort(0)) {
                // Copy buffer into a new allocated buffer. This lets us return the original buffer with dataReturnOut,
                // and AuthenticationRouter can handle the deallocation of the unknown buffer when it returns on
                // bufferReturnIn
                Fw::Buffer packetBufferCopy = this->bufferAllocate_out(0, packetBuffer.getSize());
                auto copySerializer = packetBufferCopy.getSerializer();
                status = copySerializer.serializeFrom(packetBuffer.getData(), packetBuffer.getSize(),
                                                      Fw::Serialization::OMIT_LENGTH);
                FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
                // Send the copied buffer out. It will come back on fileBufferReturnIn once the receiver is done with it
                this->unknownDataOut_out(0, packetBufferCopy, context);
            }
        }
    }

    // Return ownership of the incoming packetBuffer
    this->dataReturnOut_out(0, packetBuffer, context);
}

void AuthenticationRouter ::cmdResponseIn_handler(FwIndexType portNum,
                                                  FwOpcodeType opcode,
                                                  U32 cmdSeq,
                                                  const Fw::CmdResponse& response) {
    (void)portNum;
    (void)opcode;
    (void)cmdSeq;
    (void)response;
    // This is a no-op because AuthenticationRouter does not need to handle command responses
    // but the port must be connected
}

void AuthenticationRouter ::fileBufferReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->bufferDeallocate_out(0, fwBuffer);
}

}  // namespace Svc
