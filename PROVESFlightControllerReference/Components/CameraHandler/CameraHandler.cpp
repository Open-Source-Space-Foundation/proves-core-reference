// ======================================================================
// \title  CameraHandler.cpp
// \author moises
// \brief  cpp file for CameraHandler component implementation class
//         Handles camera protocol processing and image file saving
// ======================================================================

#include "PROVESFlightControllerReference/Components/CameraHandler/CameraHandler.hpp"

#include <cstdio>
#include <cstring>

#include "Fw/Types/Assert.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "Os/File.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CameraHandler ::CameraHandler(const char* const compName) : CameraHandlerComponentBase(compName) {}

CameraHandler ::~CameraHandler() {
    // Close file if still open
    if (m_fileOpen) {
        m_file.close();
        m_fileOpen = false;
    }

    U32 count = 0;
    if (!readImageCount(count)) {
        count = 0;
        writeImageCount(count);
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void CameraHandler ::dataIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {
    // Check if we received data successfully
    if (status != Drv::ByteStreamStatus::OP_OK) {
        // Log error and abort if receiving
        if (m_parser.isReceiving() && m_fileOpen) {
            handleFileError();
        }
        // NOTE: PayloadCom will handle buffer return, not us
        return;
    }

    // Check if buffer is valid
    if (!buffer.isValid()) {
        return;
    }

    // Get the data from the buffer (we don't own it, just read it)
    const U8* data = buffer.getData();
    U32 dataSize = static_cast<U32>(buffer.getSize());

    // Emit telemetry to track state at entry to handler
    this->tlmWrite_BytesReceived(m_parser.bytesReceived());
    this->tlmWrite_ExpectedSize(m_parser.expectedSize());
    this->tlmWrite_IsReceiving(m_parser.isReceiving());
    this->tlmWrite_FileOpen(m_fileOpen);

    if (m_parser.isReceiving() && m_fileOpen) {
        // Currently receiving image data - write directly to file
        const ImageProtocolParser::ChunkPlan plan = m_parser.planChunk(dataSize);

        if (plan.complete) {
            // Image is complete!
            finalizeImageTransfer();

            // If there's extra data after the image (e.g., <IMG_END> or next header),
            // push it to protocol buffer for processing
            if (dataSize > 0) {
                handleProtocolResult(m_parser.feedAfterImage(data, dataSize));
            }

            // Transfer is complete - don't try to write anything more
            return;
        }

        // Write chunk to file
        if (!writeChunkToFile(data, plan.toWrite)) {
            // Write failed
            this->log_WARNING_HI_CommandError(Fw::LogStringArg("File write failed"));
            handleFileError();
            return;
        }

        const U8 milestone = m_parser.onChunkWritten(plan.toWrite);

        // Emit telemetry after each write
        this->tlmWrite_BytesReceived(m_parser.bytesReceived());
        this->tlmWrite_ExpectedSize(m_parser.expectedSize());

        // Emit progress events at 25%, 50%, 75% milestones
        if (milestone != 0) {
            this->log_ACTIVITY_HI_ImageTransferProgress(milestone, m_parser.bytesReceived(), m_parser.expectedSize());
        }
    } else {
        // Not receiving image - look for image headers and commands
        handleProtocolResult(m_parser.feed(data, dataSize));
    }

    // NOTE: Do NOT return buffer here - PayloadCom owns the buffer and will return it
    // Returning it twice causes buffer management issues
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void CameraHandler ::TAKE_IMAGE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const char* takeImageCmd = "snap";
    SEND_COMMAND_cmdHandler(opCode, cmdSeq, Fw::CmdStringArg(takeImageCmd));
}

void CameraHandler ::PING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (this->m_parser.isReceiving()) {
        this->log_WARNING_LO_FailedCommandCurrentlyReceiving();
        return;
    }
    this->m_waiting_for_pong = true;
    const char* pingCmd = "ping";
    SEND_COMMAND_cmdHandler(opCode, cmdSeq, Fw::CmdStringArg(pingCmd));
}

void CameraHandler ::SEND_COMMAND_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdStringArg& cmd) {
    // Append newline to command to send to PayloadCom
    Fw::CmdStringArg tempCmd = cmd;
    tempCmd += "\n";
    Fw::Buffer commandBuffer(reinterpret_cast<U8*>(const_cast<char*>(tempCmd.toChar())), tempCmd.length());

    // Send command to PayloadCom (which will forward to UART)
    // ByteStreamData ports require buffer and status
    this->commandOut_out(0, commandBuffer, Drv::ByteStreamStatus::OP_OK);

    Fw::LogStringArg logCmd(cmd);
    this->log_ACTIVITY_HI_CommandSuccess(logCmd);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Helper method implementations
// ----------------------------------------------------------------------

void CameraHandler ::handleProtocolResult(const ImageProtocolParser::Result& result) {
    switch (result.found) {
        case ImageProtocolParser::Found::PONG:
            if (this->m_waiting_for_pong) {
                this->log_ACTIVITY_HI_PongReceived();
                this->m_waiting_for_pong = false;
            } else {
                this->log_WARNING_HI_BadPongReceived();
            }
            break;
        case ImageProtocolParser::Found::IMAGE_HEADER:
            this->startImageTransfer(result.imageSize);
            break;
        case ImageProtocolParser::Found::NOTHING:
            break;
    }
}

void CameraHandler ::startImageTransfer(U32 imageSize) {
    // Valid header! Open file immediately for streaming
    U32 count = 0;

    // Read image count from file
    if (!readImageCount(count)) {
        count = 0;  // If read fails, start from 0
        writeImageCount(count);
    }

    // Generate filename - save to root filesystem
    char filename[64];
    // Get parameter for image number
    snprintf(filename, sizeof(filename), "/cam%03d_img_%03d.jpg", this->cam_number, count + 1);
    m_currentFilename = filename;

    writeImageCount(count + 1);

    // Open file for writing
    Os::File::Status status = m_file.open(m_currentFilename.c_str(), Os::File::OPEN_WRITE);

    if (status != Os::File::OP_OK) {
        // Failed to open file
        this->log_WARNING_HI_CommandError(Fw::LogStringArg("Failed to open file"));
        m_parser.rejectHeader();
        return;
    }

    m_fileOpen = true;

    // Log transfer started event
    this->log_ACTIVITY_HI_ImageTransferStarted(imageSize);

    // Emit telemetry after opening file
    this->tlmWrite_BytesReceived(m_parser.bytesReceived());
    this->tlmWrite_ExpectedSize(m_parser.expectedSize());
    this->tlmWrite_IsReceiving(m_parser.isReceiving());
    this->tlmWrite_FileOpen(m_fileOpen);

    // NOTE: PayloadCom sends ACK automatically after forwarding data
    // No need to send ACK here - that's handled by the communication layer

    // Write any remaining data (image data) directly to file
    // NOTE: This should be empty since camera waits for ACK before sending data
    const ImageProtocolParser::Chunk chunk = m_parser.takeBufferedImageData();
    if (chunk.present) {
        if (writeChunkToFile(chunk.data, chunk.size)) {
            m_parser.onBufferedDataWritten(chunk.size);

            // Check if complete already
            if (m_parser.isComplete()) {
                finalizeImageTransfer();
            }
        } else {
            handleFileError();
        }

        m_parser.clearBuffer();
    }
}

bool CameraHandler ::writeChunkToFile(const U8* data, U32 size) {
    if (!m_fileOpen || size == 0) {
        return false;
    }

    // Write data to file, handling partial writes
    U32 totalWritten = 0;
    const U8* ptr = data;

    while (totalWritten < size) {
        FwSizeType toWrite = static_cast<FwSizeType>(size - totalWritten);
        Os::File::Status status = m_file.write(ptr, toWrite, Os::File::WaitType::WAIT);

        if (status != Os::File::OP_OK) {
            return false;
        }

        // toWrite now contains the actual bytes written
        totalWritten += static_cast<U32>(toWrite);
        ptr += toWrite;
    }

    // Log bytes written
    // this->log_ACTIVITY_LO_ChunkWritten(totalWritten);

    return true;
}

void CameraHandler ::finalizeImageTransfer() {
    if (!m_fileOpen) {
        return;
    }

    // Close the file
    m_file.close();
    m_fileOpen = false;

    // Increment success counter
    m_images_saved++;

    // Log transfer complete event with path and size
    Fw::LogStringArg pathArg(m_currentFilename.c_str());
    this->log_ACTIVITY_HI_ImageTransferComplete(pathArg, m_parser.bytesReceived());

    // NOTE: PayloadCom sends ACK automatically - no need to send here

    // Reset state
    m_parser.finishTransfer();

    // Emit telemetry after finalizing
    this->tlmWrite_BytesReceived(m_parser.bytesReceived());
    this->tlmWrite_ExpectedSize(m_parser.expectedSize());
    this->tlmWrite_IsReceiving(m_parser.isReceiving());
    this->tlmWrite_FileOpen(m_fileOpen);
    this->tlmWrite_ImagesSaved(m_images_saved);
}

void CameraHandler ::handleFileError() {
    // Close file if open
    if (m_fileOpen) {
        m_file.close();
        m_fileOpen = false;
    }

    // Increment error counter
    m_file_error_count++;

    // Log error
    this->log_WARNING_HI_CommandError(Fw::LogStringArg("File write error"));

    // Reset state
    m_parser.abortTransfer();

    // Emit telemetry after error handling
    this->tlmWrite_BytesReceived(m_parser.bytesReceived());
    this->tlmWrite_ExpectedSize(m_parser.expectedSize());
    this->tlmWrite_IsReceiving(m_parser.isReceiving());
    this->tlmWrite_FileOpen(m_fileOpen);
    this->tlmWrite_FileErrorCount(m_file_error_count);
}

bool CameraHandler ::readImageCount(U32& count) {
    Os::File file;
    U8 buffer[sizeof(U32)];
    Fw::ExternalSerializeBuffer deserializer(buffer, sizeof(buffer));

    // Open the file for reading
    Os::File::Status status = file.open(IMAGE_COUNT_PATH, Os::File::OPEN_READ);
    if (status != Os::File::OP_OK) {
        file.close();
        this->log_WARNING_HI_FileReadError();
        return false;
    }

    FwSizeType size = sizeof(buffer);
    status = file.read(buffer, size);
    file.close();

    if (status != Os::File::OP_OK || size != sizeof(buffer)) {
        this->log_WARNING_HI_FileReadError();
        return false;
    }

    deserializer.setBuffLen(size);
    Fw::SerializeStatus serialize_status = deserializer.deserializeTo(count);
    if (serialize_status != Fw::SerializeStatus::FW_SERIALIZE_OK) {
        this->log_WARNING_HI_FileReadError();
        return false;
    }
    return true;
}

bool CameraHandler ::writeImageCount(U32 count) {
    Os::File file;
    U8 buffer[sizeof(U32)];
    Fw::ExternalSerializeBuffer serializer(buffer, sizeof(buffer));

    Fw::SerializeStatus serialize_status = serializer.serializeFrom(count);
    FW_ASSERT(serialize_status == Fw::SerializeStatus::FW_SERIALIZE_OK);

    // Open the file for reading, and continue only if successful
    Os::File::Status status = file.open(IMAGE_COUNT_PATH, Os::File::OPEN_CREATE, Os::File::OVERWRITE);
    if (status != Os::File::OP_OK) {
        this->log_WARNING_HI_FileWriteError();
        file.close();
        return false;
    }

    FwSizeType size = sizeof(buffer);
    status = file.write(buffer, size);
    file.close();
    return (status == Os::File::OP_OK && size == sizeof(buffer));
}

}  // namespace Components
