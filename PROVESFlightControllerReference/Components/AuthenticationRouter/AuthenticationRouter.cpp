// ======================================================================
// \title  AuthenticationRouter.cpp
// \author Ines (based on thomas-bc's FprimeRouter.cpp)
// \brief  cpp file for AuthenticationRouter component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/AuthenticationRouter/AuthenticationRouter.hpp"

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

// List of opcodes (as hex strings) that bypass authentication
// Format: 8 hex characters (4 bytes = 32-bit opcode)
// Example: "00000001" for opcode 0x00000001

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
    // Lock the mutex to prevent multiple threads from updating the command loss start time simultaneously
    Os::ScopeLock lock(this->m_commandLossMutex);

    Fw::Time current_time = this->getTime();

    // If writing (command received), reset the timer to current time
    // On boot, m_commandLossStartTime starts at ZERO_TIME, so first command will set it
    // Also reset if timebase changed (can't compare times with different timebases)
    bool changed_time_base = this->m_commandLossStartTime.getTimeBase() != current_time.getTimeBase();
    if (write_to_file || this->m_commandLossStartTime == Fw::ZERO_TIME || changed_time_base) {
        this->m_commandLossStartTime = current_time;
        return current_time;
    }

    // If reading (checking timer), return the stored start time
    return this->m_commandLossStartTime;
}

void AuthenticationRouter ::fileBufferReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->bufferDeallocate_out(0, fwBuffer);
}

}  // namespace Svc
