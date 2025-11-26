// ======================================================================
// \title  AuthenticationRouter.cpp
// \author Ines (based on thomas-bc's FprimeRouter.cpp)
// \brief  cpp file for AuthenticationRouter component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/AuthenticationRouter/AuthenticationRouter.hpp"

#include <atomic>
#include <chrono>
#include <string>

#include "Fw/Com/ComPacket.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Os/File.hpp"
#include "config/ApidEnumAc.hpp"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

constexpr const char LAST_LOSS_TIME_FILE[] = "//loss_max_time.txt";

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AuthenticationRouter ::AuthenticationRouter(const char* const compName) : AuthenticationRouterComponentBase(compName) {
    U32 last_loss_time = this->initializeFiles(LAST_LOSS_TIME_FILE);
    printk("Last loss time: %d\n", last_loss_time);
}
AuthenticationRouter ::~AuthenticationRouter() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void AuthenticationRouter ::schedIn_handler(FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;

    // // Check if the last loss time is past the current time
    // U32 last_loss_time = this->readFromFile(LAST_LOSS_TIME_FILE);
    // U32 current_time = // TODO: Get time from the rtc here
    // if (current_time - last_loss_time > this->get_LOSS_MAX_TIME_param()) {
    //     this->log_WARNING_HI_CommandLossTimeExpired();
    //     // TODO: Send out to safemode
    // }

    U32 last_loss_time_from_file = this->readFromFile(LAST_LOSS_TIME_FILE);
    printk("Last loss time from file: %d\n", last_loss_time_from_file);

    printk("Now writing new time to file\n");
    this->writeToFile(LAST_LOSS_TIME_FILE, last_loss_time_from_file + 1);
    printk("New time written to file: %d\n", last_loss_time_from_file + 1);

    printk("Now reading time from file\n");
    U32 time_from_file = this->readFromFile(LAST_LOSS_TIME_FILE);
    printk("Time from file: %d\n", time_from_file);
}

U32 AuthenticationRouter ::getTimeFromRTC() {
    // TODO: Get time from the rtc here
    return 0;
}

U32 AuthenticationRouter ::writeToFile(const char* filePath, U32 time) {
    // TO DO: Add File Opening Error Handling here and in all the other file functions
    Os::File file;
    Os::File::Status openStatus = file.open(filePath, Os::File::OPEN_CREATE, Os::File::OverwriteType::NO_OVERWRITE);
    if (openStatus == Os::File::OP_OK) {
        const U8* buffer = reinterpret_cast<const U8*>(&time);
        FwSizeType size = static_cast<FwSizeType>(sizeof(time));
        (void)file.write(buffer, size, Os::File::WaitType::WAIT);
        file.close();
    }
    return time;
}

U32 AuthenticationRouter ::readFromFile(const char* filePath) {
    Os::File file;
    U32 time = 0;
    Os::File::Status openStatus = file.open(filePath, Os::File::OPEN_READ);
    if (openStatus == Os::File::OP_OK) {
        FwSizeType size = static_cast<FwSizeType>(sizeof(time));
        FwSizeType expectedSize = size;
        Os::File::Status readStatus = file.read(reinterpret_cast<U8*>(&time), size, Os::File::WaitType::WAIT);
        file.close();
        if (readStatus == Os::File::OP_OK && size == expectedSize) {
            return time;
        }
    }
    return time;
}

U32 AuthenticationRouter ::initializeFiles(const char* filePath) {
    U32 last_loss_time = 0;  // TO DO GET TIME FROM THE RTC HERE
    bool loadedFromFile = false;

    Os::File file;

    // Check if the file exists and is readable
    Os::File::Status openStatus = file.open(filePath, Os::File::OPEN_READ);
    if (openStatus == Os::File::OP_OK) {
        FwSizeType size = static_cast<FwSizeType>(sizeof(last_loss_time));
        FwSizeType expectedSize = size;
        Os::File::Status readStatus = file.read(reinterpret_cast<U8*>(&last_loss_time), size, Os::File::WaitType::WAIT);
        file.close();
        loadedFromFile = (readStatus == Os::File::OP_OK) && (size == expectedSize);
    }

    // If the file doesn't exist, create it
    if (!loadedFromFile) {
        Os::File::Status createStatus =
            file.open(filePath, Os::File::OPEN_CREATE, Os::File::OverwriteType::NO_OVERWRITE);
        if (createStatus == Os::File::OP_OK) {
            const U8* buffer = reinterpret_cast<const U8*>(&last_loss_time);
            FwSizeType size = static_cast<FwSizeType>(sizeof(last_loss_time));
            (void)file.write(buffer, size, Os::File::WaitType::WAIT);
            file.close();
        }
    }

    return last_loss_time;
}

void AuthenticationRouter ::dataIn_handler(FwIndexType portNum,
                                           Fw::Buffer& packetBuffer,
                                           const ComCfg::FrameContext& context) {
    printk("AuthenticationRouter ::dataIn_handler\n");
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
