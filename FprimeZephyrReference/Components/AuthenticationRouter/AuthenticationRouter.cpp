// ======================================================================
// \title  AuthenticationRouter.cpp
// \author Ines (based on thomas-bc's FprimeRouter.cpp)
// \brief  cpp file for AuthenticationRouter component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/AuthenticationRouter/AuthenticationRouter.hpp"

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Log/LogString.hpp>
#include <Fw/Time/Time.hpp>
#include <Fw/Types/String.hpp>

#include "Fw/Com/ComPacket.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Os/File.hpp"
#include "config/ApidEnumAc.hpp"
#include <zephyr/drivers/rtc.h>

constexpr const U8 OP_CODE_LENGTH = 4;  // F Prime opcodes are 32-bit (4 bytes)
constexpr const U8 OP_CODE_START = 2;   // Opcode starts at byte offset 2 in the packet buffer

// List of opcodes (as hex strings) that bypass authentication
// Format: 8 hex characters (4 bytes = 32-bit opcode)
// Example: "00000001" for opcode 0x00000001
static constexpr U32 kBypassOpCodes[] = {
    0x01000000,  // no op
    0x2200B000,  // get sequence number
    0x10065000   // amateur name
};
constexpr size_t kBypassOpCodeCount = sizeof(kBypassOpCodes) / sizeof(kBypassOpCodes[0]);

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AuthenticationRouter ::AuthenticationRouter(const char* const compName)
    : AuthenticationRouterComponentBase(compName), m_safeModeCalled(false), m_commandLossStartTime(Fw::ZERO_TIME) {}
AuthenticationRouter ::~AuthenticationRouter() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

bool AuthenticationRouter::BypassesAuthentification(Fw::Buffer& packetBuffer) {
    // Check bounds before extracting
    if (packetBuffer.getSize() < (OP_CODE_START + OP_CODE_LENGTH)) {
        return false;
    }

    // Extract opcode bytes
    U8 opCodeBytes[OP_CODE_LENGTH];
    std::memcpy(opCodeBytes, packetBuffer.getData() + OP_CODE_START, OP_CODE_LENGTH);

    // Combine opcode bytes into a single 32-bit value for comparison
    const U32 opCode = (static_cast<U32>(opCodeBytes[0]) << 24) | (static_cast<U32>(opCodeBytes[1]) << 16) |
                       (static_cast<U32>(opCodeBytes[2]) << 8) | static_cast<U32>(opCodeBytes[3]);

    // Check if opcode matches any in the bypass list
    for (size_t i = 0; i < kBypassOpCodeCount; i++) {
        if (opCode == kBypassOpCodes[i]) {
            return true;
        }
    }

    return false;
}

void AuthenticationRouter ::CallSafeMode() {
    // Call Safe mode with EXTERNAL_REQUEST reason (command loss is an external component request)
    log_WARNING_HI_CommandLossFileInitFailure_ThrottleClear();

    // Only the Lora is connetcted to the watchdog, so check connections to prevent fault
    // should never happen bc Sband and UART are not connected to the rate group, but just in case
    if (this->isConnected_reset_watchdog_OutputPort(0)) {
        this->reset_watchdog_out(0);
    }

    // write current time to file
    this->update_command_loss_start(true);

    // Since it takes 26 seconds for the watchdog to reboot the system, we set safe mode after resetting the watchdog,
    // it should boot back into safe mode

    this->SetSafeMode_out(0, Components::SafeModeReason::EXTERNAL_REQUEST);
}

void AuthenticationRouter ::dataIn_handler(FwIndexType portNum,
                                           Fw::Buffer& packetBuffer,
                                           const ComCfg::FrameContext& context) {
    // Check if the OpCodes are in the OpCode list
    // TODO
    bool bypasses = this->BypassesAuthentification(packetBuffer);

    // the packet was not authenticated
    if (context.get_authenticated() == 0 && !bypasses) {
        this->dataReturnOut_out(0, packetBuffer, context);
        return;
    }

    this->update_command_loss_start(true);
    // Reset safe mode flag when a new command is received
    this->m_safeModeCalled = false;

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

void AuthenticationRouter ::run_handler(FwIndexType portNum, U32 context) {
    Fw::Time command_loss_start = this->update_command_loss_start();

    Fw::ParamValid is_valid;
    Fw::TimeIntervalValue command_loss_period = this->paramGet_COMM_LOSS_TIME(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);
    Fw::Time command_loss_interval(command_loss_start.getTimeBase(), command_loss_period.get_seconds(),
                                   command_loss_period.get_useconds());
    Fw::Time command_loss_end = Fw::Time::add(command_loss_start, command_loss_interval);

    Fw::Time current_time =
        (command_loss_end.getTimeBase() == TimeBase::TB_PROC_TIME) ? this->get_uptime() : this->getTime();

    if (current_time > command_loss_end && !this->m_safeModeCalled) {
        this->log_WARNING_HI_CommandLossFound(Fw::Time::sub(current_time, command_loss_start).getSeconds());
        this->CallSafeMode();
        this->m_safeModeCalled = true;
    }
}

Fw::Time AuthenticationRouter ::get_uptime() {
    uint32_t seconds = k_uptime_seconds();
    Fw::Time time(TimeBase::TB_PROC_TIME, 0, static_cast<U32>(seconds), 0);
    return time;
}

void AuthenticationRouter ::GET_COMMAND_LOSS_DATA_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::Time current_time = this->getTime();
    Fw::Time command_loss_start = this->update_command_loss_start();
    Fw::ParamValid is_valid;
    Fw::TimeIntervalValue command_loss_period = this->paramGet_COMM_LOSS_TIME(is_valid);
    FW_ASSERT(is_valid == Fw::ParamValid::VALID || is_valid == Fw::ParamValid::DEFAULT);
    Fw::Time command_loss_interval(command_loss_start.getTimeBase(), command_loss_period.get_seconds(),
                                   command_loss_period.get_useconds());
    Fw::Time command_loss_end = Fw::Time::add(command_loss_start, command_loss_interval);
    this->log_ACTIVITY_LO_EmitCommandLossData(command_loss_start.getSeconds(), current_time.getSeconds(),
                                              command_loss_interval.getSeconds(), command_loss_end.getSeconds(),
                                              this->m_safeModeCalled);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

Fw::Time AuthenticationRouter ::update_command_loss_start(bool write_to_file) {
    Os::ScopeLock lock(this->m_commandLossMutex);

    // Update file with current time and cache it
    Fw::Time current_time = this->getTime();

    // if current time base if monotonic, we don't want to write it to file, but we still want to update the cached
    // time and return it this way we never write monotonic time to file, which would be invalid on reboot and if
    // the system is using monotonic time, we don't consistently return a previously saved workstation time to a
    // cube stuck on monotonic (ie broken RTC). So we don't write monotonic time to file, but cache it for use in
    // current session

    if (current_time.getTimeBase() == TimeBase::TB_PROC_TIME) {
        if (write_to_file) {
            // Don't write monotonic time to file, but cache it for use in current session
            this->m_commandLossStartTime = current_time;
            return current_time;
        } else {
            // Return cached time (the time when last command arrived)
            return this->m_commandLossStartTime;
        }
    }

    Fw::ParamValid is_valid;
    auto time_file = this->paramGet_COMM_LOSS_TIME_START_FILE(is_valid);

    if (write_to_file) {
        Os::File::Status status = Utilities::FileHelper::writeToFile(time_file.toChar(), current_time);
        if (status != Os::File::OP_OK) {
            this->log_WARNING_HI_CommandLossFileInitFailure();
        }
        this->m_commandLossStartTime = current_time;

        return current_time;
    } else {
        // Check if we need to load from file (cache is zero/uninitialized or timebase mismatch with the file)
        // Otherwise we want to read from the cache in case the filesystem is broken
        // Also invalidate cache if timebase changed (e.g., system switched from monotonic to workstation time)
        if (this->m_commandLossStartTime == Fw::ZERO_TIME ||
            this->m_commandLossStartTime.getTimeBase() != current_time.getTimeBase()) {
            // Read stored time from file, or use current time if file doesn't exist
            Fw::Time time = this->getTime();
            Os::File::Status status = Utilities::FileHelper::readFromFile(time_file.toChar(), time);

            // On read failure, write the current time to the file for future reads
            if (status != Os::File::OP_OK) {
                status = Utilities::FileHelper::writeToFile(time_file.toChar(), time);
                if (status != Os::File::OP_OK) {
                    this->log_WARNING_HI_CommandLossFileInitFailure();
                }
            }
            // Cache the loaded time
            this->m_commandLossStartTime = time;
        }
        // Return cached time
        return this->m_commandLossStartTime;
    }
}

void AuthenticationRouter ::fileBufferReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->bufferDeallocate_out(0, fwBuffer);
}

}  // namespace Svc
