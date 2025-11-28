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
constexpr const char LAST_LOSS_TIME_FILE_MONOTONIC[] = "//loss_max_time_monotonic.txt";
constexpr const char COMMAND_LOSS_EXPIRED_FLAG_FILE[] = "//command_loss_expired_flag.txt";
constexpr const char BYPASS_AUTHENTICATION_FILE[] = "//bypass_authentification_file.txt";
constexpr const U8 OP_CODE_LENGTH = 8;  // TO DO: Double check len
constexpr const U8 OP_CODE_START = 6;   // TO DO: Check if this is the len of the security packet or smth else between

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AuthenticationRouter ::AuthenticationRouter(const char* const compName) : AuthenticationRouterComponentBase(compName) {
    // Initialize previous time type flag by checking current time type
    Fw::Time time = this->getTime();
    TimeBase b = time.getTimeBase();
    m_previousTypeTimeFlag = (b == TimeBase::TB_PROC_TIME);  // true if monotonic, false if RTC
    m_TypeTimeFlag = m_previousTypeTimeFlag;                 // Initialize current flag to match

    this->initializeFiles(LAST_LOSS_TIME_FILE);
    this->initializeFiles(LAST_LOSS_TIME_FILE_MONOTONIC);

    // Load the command loss expired flag from file (persists across boots)
    m_commandLossTimeExpiredLogged = this->readCommandLossExpiredFlag();
}
AuthenticationRouter ::~AuthenticationRouter() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void AuthenticationRouter ::schedIn_handler(FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;

    U32 current_loss_time = this->getTimeFromRTC();

    // Check if time type has switched from RTC to monotonic or vice versa
    // If switched, update the appropriate file to prevent false timeout triggers
    if (m_previousTypeTimeFlag != m_TypeTimeFlag) {
        if (m_previousTypeTimeFlag == false && m_TypeTimeFlag == true) {
            // Switched from RTC to monotonic - update monotonic file
            this->writeToFile(LAST_LOSS_TIME_FILE_MONOTONIC, current_loss_time);
        } else if (m_previousTypeTimeFlag == true && m_TypeTimeFlag == false) {
            // Switched from monotonic to RTC - update RTC file
            this->writeToFile(LAST_LOSS_TIME_FILE, current_loss_time);
        }
        // Update previous flag to current flag
        m_previousTypeTimeFlag = m_TypeTimeFlag;
    }

    // check if the time is RTC or monotonic and set the flag accordingly
    // Read from the appropriate file based on the time type
    U32 last_loss_time;
    if (m_TypeTimeFlag == true) {
        last_loss_time = this->readFromFile(LAST_LOSS_TIME_FILE_MONOTONIC);
        // Don't overwrite the file here - initializeFiles() should have already handled initialization
        // If last_loss_time is 0, it means the file doesn't exist or read failed, but we don't want to
        // overwrite on every schedIn call. The file should only be written when commands are received.
    } else {
        last_loss_time = this->readFromFile(LAST_LOSS_TIME_FILE);
        // Don't overwrite the file here - initializeFiles() should have already handled initialization
        // If last_loss_time is 0, it means the file doesn't exist or read failed, but we don't want to
        // overwrite on every schedIn call. The file should only be written when commands are received.
    }

    // // Check if the last loss time is past the current time

    // Get the LOSS_MAX_TIME parameter
    Fw::ParamValid valid;
    U32 loss_max_time = this->paramGet_LOSS_MAX_TIME(valid);

    if (current_loss_time >= last_loss_time && (current_loss_time - last_loss_time) > loss_max_time) {
        // Timeout condition is met
        if (m_commandLossTimeExpiredLogged == false) {
            this->log_ACTIVITY_HI_CommandLossTimeExpired(Fw::On::ON);
            m_commandLossTimeExpiredLogged = true;
            this->writeCommandLossExpiredFlag(true);  // Persist flag to file
            this->tlmWrite_CommandLossSafeOn(true);
            // Only send safemode signal if port is connected
            if (this->isConnected_SafeModeOn_OutputPort(0)) {
                this->SafeModeOn_out(0);
            }
        }
        // Flag stays true - signal will not be sent again until a command is received
    } else {
        // Timeout condition is NOT met - reset the flag so event can trigger again if timeout occurs later
        if (m_commandLossTimeExpiredLogged == true) {
            m_commandLossTimeExpiredLogged = false;
            this->writeCommandLossExpiredFlag(false);  // Persist flag to file
        }
    }
}

U32 AuthenticationRouter ::getTimeFromRTC() {
    // TODO: Get time from the rtc here
    // use the RtcManager timeGetPort to get the time
    // getTime() automatically calls the timeCaller port which is connected to RtcManager
    Fw::Time time = this->getTime();

    // Check if the time is RTC or monotonic and set the flag accordingly
    TimeBase b = time.getTimeBase();
    if (b == TimeBase::TB_PROC_TIME) {
        // monotonic time
        m_TypeTimeFlag = true;
    } else {
        // RTC time
        m_TypeTimeFlag = false;
    }

    return time.getSeconds();
}

U32 AuthenticationRouter ::writeToFile(const char* filePath, U32 time) {
    // TO DO: Add File Opening Error Handling here and in all the other file functions
    Os::File file;
    // Use OPEN_CREATE with OVERWRITE to ensure the file is properly overwritten
    // This ensures data persists across boots and the file is fully overwritten
    Os::File::Status openStatus = file.open(filePath, Os::File::OPEN_CREATE, Os::File::OverwriteType::OVERWRITE);
    if (openStatus == Os::File::OP_OK) {
        const U8* buffer = reinterpret_cast<const U8*>(&time);
        FwSizeType size = static_cast<FwSizeType>(sizeof(time));
        // Use WAIT to ensure data is synced to disk (fsync/flush)
        (void)file.write(buffer, size, Os::File::WaitType::WAIT);
        file.close();
        // Note: Specific reason for overwrite is printed at the call site
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

bool AuthenticationRouter ::readCommandLossExpiredFlag() {
    Os::File file;
    bool flag = false;
    Os::File::Status openStatus = file.open(COMMAND_LOSS_EXPIRED_FLAG_FILE, Os::File::OPEN_READ);
    if (openStatus == Os::File::OP_OK) {
        FwSizeType size = static_cast<FwSizeType>(sizeof(flag));
        FwSizeType expectedSize = size;
        Os::File::Status readStatus = file.read(reinterpret_cast<U8*>(&flag), size, Os::File::WaitType::WAIT);
        file.close();
        if (readStatus == Os::File::OP_OK && size == expectedSize) {
            return flag;
        }
    }
    // File doesn't exist or read failed - return false (default)
    return false;
}

void AuthenticationRouter ::writeCommandLossExpiredFlag(bool flag) {
    Os::File file;
    Os::File::Status openStatus =
        file.open(COMMAND_LOSS_EXPIRED_FLAG_FILE, Os::File::OPEN_CREATE, Os::File::OverwriteType::OVERWRITE);
    if (openStatus == Os::File::OP_OK) {
        const U8* buffer = reinterpret_cast<const U8*>(&flag);
        FwSizeType size = static_cast<FwSizeType>(sizeof(flag));
        (void)file.write(buffer, size, Os::File::WaitType::WAIT);
        file.close();
    }
}

U32 AuthenticationRouter ::initializeFiles(const char* filePath) {
    U32 last_loss_time = this->getTimeFromRTC();
    bool loadedFromFile = false;
    bool timeIsValid = false;

    Os::File file;

    // Check if the file exists and is readable
    Os::File::Status openStatus = file.open(filePath, Os::File::OPEN_READ);
    if (openStatus == Os::File::OP_OK) {
        FwSizeType size = static_cast<FwSizeType>(sizeof(last_loss_time));
        FwSizeType expectedSize = size;
        Os::File::Status readStatus = file.read(reinterpret_cast<U8*>(&last_loss_time), size, Os::File::WaitType::WAIT);
        file.close();
        loadedFromFile = (readStatus == Os::File::OP_OK) && (size == expectedSize);

        // Validate the stored time - only check if it's 0 (RTC wasn't initialized when written)
        // If it's not 0, preserve it regardless of value (no range checking)
        if (loadedFromFile) {
            // If time is 0, it means RTC wasn't initialized when file was written - should overwrite
            if (last_loss_time == 0) {
                timeIsValid = false;  // Force overwrite
            } else {
                // Any non-zero value is considered valid - preserve it
                timeIsValid = true;
            }
        }
    }

    // Create or overwrite file if:
    // 1. File doesn't exist, OR
    // 2. File contains 0 (RTC wasn't initialized when written), OR
    // 3. File contains invalid time
    if (!loadedFromFile || !timeIsValid) {
        last_loss_time = this->getTimeFromRTC();
        Os::File::Status createStatus = file.open(filePath, Os::File::OPEN_CREATE, Os::File::OverwriteType::OVERWRITE);
        if (createStatus == Os::File::OP_OK) {
            const U8* buffer = reinterpret_cast<const U8*>(&last_loss_time);
            FwSizeType size = static_cast<FwSizeType>(sizeof(last_loss_time));
            (void)file.write(buffer, size, Os::File::WaitType::WAIT);
            file.close();
        }
    }

    return last_loss_time;
}

bool AuthenticationRouter ::BypassesAuthentification(Fw::Buffer& packetBuffer) {
    // TO DO: Fill this in with reading the file and searching through it (see authenticate)
    std::string& opCode;
    std::memcpy(OpCode, packetBuffer.getData() + 6, OP_CODE_LENGTH);

    // Open file
    Os::File::bypassOpCodesFile;
    Os::File::Status openStatus = bypassOpCodesFile.open(BYPASS_AUTHENTICATION_FILE, Os::File::OPEN_READ);
    // if the file does not exist return false; TO DO CHECK WITH ERROR CHECKING
    if (openStatus != Os::File::OP_OK) {
        this->log_WARNING_HI_FileOpenError(openStatus);
        return config;
    }

    // check if opcode is in file

    return true;
}

void AuthenticationRouter ::dataIn_handler(FwIndexType portNum,
                                           Fw::Buffer& packetBuffer,
                                           const ComCfg::FrameContext& context) {
    // Any packet received resets the flag and updates the last loss time
    printk("context %d", context.get_authenticated());

    // Check if the OpCodes are in the OpCode list
    // TODO
    bool bypasses = this->BypassesAuthentification(packetBuffer);

    // the packet was not authenticated
    if (context.get_authenticated() == 0 && bypasses == false) {
        // emit reject packet event
        this->log_ACTIVITY_LO_PassedRouter(0);
        // Return ownership of the incoming packetBuffer
        this->dataReturnOut_out(0, packetBuffer, context);
        return;
    }

    if (bypasses == true) {
        // emit bypass event
        this->log_ACTIVITY_LO_BypassedAuthentification();
    }

    U32 current_time = this->getTimeFromRTC();
    if (m_TypeTimeFlag == true) {
        this->writeToFile(LAST_LOSS_TIME_FILE_MONOTONIC, current_time);
    } else {
        this->writeToFile(LAST_LOSS_TIME_FILE, current_time);
    }
    // Reset the flag when any packet is received
    m_commandLossTimeExpiredLogged = false;
    this->writeCommandLossExpiredFlag(false);  // Persist flag to file
    // Update telemetry with the command loss safe on status
    this->tlmWrite_CommandLossSafeOn(false);
    this->tlmWrite_LastCommandPacketTime(static_cast<U64>(current_time));

    Fw::SerializeStatus status;
    Fw::ComPacketType packetType = context.get_apid();
    // Route based on received APID (packet type)
    switch (packetType) {
        // Handle a command packet
        case Fw::ComPacketType::FW_PACKET_COMMAND: {
            // Update telemetry with the last command packet time
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
