// ======================================================================
// \title  PayloadHandler.cpp
// \author robertpendergrast
// \brief  cpp file for PayloadHandler component implementation class
// ======================================================================
#include "Os/File.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "FprimeZephyrReference/Components/PayloadHandler/PayloadHandler.hpp"
#include <cstring>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PayloadHandler ::PayloadHandler(const char* const compName) 
    : PayloadHandlerComponentBase(compName),
      m_protocolBufferSize(0),
      m_imageBufferUsed(0) {
    // Initialize protocol buffer to zero
    memset(m_protocolBuffer, 0, PROTOCOL_BUFFER_SIZE);
}

PayloadHandler ::~PayloadHandler() {
    // Clean up any allocated image buffer
    deallocateImageBuffer();
}


// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------


void PayloadHandler ::in_port_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {

    this->log_ACTIVITY_LO_UartReceived();

    // Check if we received data successfully
    if (status != Drv::ByteStreamStatus::OP_OK) {
        // TODO - log error event?
        return;
    }

    // Check if buffer is valid
    if (!buffer.isValid()) {
        return;
    }

    // Get the data from the incoming buffer
    const U8* data = buffer.getData();
    const U32 dataSize = static_cast<U32>(buffer.getSize());

    // Unclear if this works as intended if data flow is interrupted

    if (m_receiving && m_imageBuffer.isValid()) {
        // Currently receiving image data - accumulate into large buffer
        // Check for end marker before accumulating
        I32 endMarkerPos = findImageEndMarker(data, dataSize);
        
        if (endMarkerPos >= 0) {
            // Found end marker - accumulate data up to marker
            U32 finalDataSize = static_cast<U32>(endMarkerPos);
            if (finalDataSize > 0) {
                if (!accumulateImageData(data, finalDataSize)) {
                    // Overflow
                    this->log_WARNING_HI_ImageDataOverflow();
                    deallocateImageBuffer();
                    m_receiving = false;
                    return;
                }
            }
            
            // Image is complete
            processCompleteImage();
        } else {
            // No end marker yet - accumulate all data
            if (!accumulateImageData(data, dataSize)) {
                // Image buffer overflow
                this->log_WARNING_HI_ImageDataOverflow();
                deallocateImageBuffer();
                m_receiving = false;
            }
        }
    } else {
        // Not receiving image - accumulate protocol data
        if (!accumulateProtocolData(data, dataSize)) {
            // Protocol buffer overflow - clear and retry
            clearProtocolBuffer();
            accumulateProtocolData(data, dataSize);
        }

        // Process protocol buffer to detect image headers/commands
        processProtocolBuffer();
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PayloadHandler ::SEND_COMMAND_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::CmdStringArg& cmd) {

    // Append newline to command to send over UART
    Fw::CmdStringArg tempCmd = cmd;  
    tempCmd += "\n";                  
    Fw::Buffer commandBuffer(
        reinterpret_cast<U8*>(const_cast<char*>(tempCmd.toChar())), 
        tempCmd.length()
    );

    // Send command over output port
    Drv::ByteStreamStatus sendStatus = this->out_port_out(0, commandBuffer);

    Fw::LogStringArg logCmd(cmd);
    
    // Log success or failure
    if (sendStatus != Drv::ByteStreamStatus::OP_OK) {
        this->log_WARNING_HI_CommandError(logCmd);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    else {
        this->log_ACTIVITY_HI_CommandSuccess(logCmd);
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Helper method implementations
// ----------------------------------------------------------------------

bool PayloadHandler ::accumulateProtocolData(const U8* data, U32 size) {
    // Check if we have space for the new data
    if (m_protocolBufferSize + size > PROTOCOL_BUFFER_SIZE) {
        return false;
    }

    // Copy data into protocol buffer
    memcpy(&m_protocolBuffer[m_protocolBufferSize], data, size);
    m_protocolBufferSize += size;

    return true;
}

void PayloadHandler ::processProtocolBuffer() {
    // Process newline-terminated commands/headers
    // Looking for "<IMG_START>" to begin image reception
    
    while (m_protocolBufferSize > 0) {
        // Look for newline character
        bool foundNewline = false;
        U32 lineEndIndex = 0;

        for (U32 i = 0; i < m_protocolBufferSize; ++i) {
            if (m_protocolBuffer[i] == '\n' || m_protocolBuffer[i] == '\r') {
                foundNewline = true;
                lineEndIndex = i;
                break;
            }
        }

        if (foundNewline) {
            U32 lineLength = lineEndIndex;
            
            // Skip carriage return if present (handle \r\n)
            // needed?
            if (lineEndIndex + 1 < m_protocolBufferSize && 
                m_protocolBuffer[lineEndIndex] == '\r' && 
                m_protocolBuffer[lineEndIndex + 1] == '\n') {
                lineEndIndex++;
            }

            // Check if this is the image start command
            if (isImageStartCommand(m_protocolBuffer, lineLength)) {
                // Allocate buffer for image data
                if (allocateImageBuffer()) {
                    m_receiving = true;
                    m_bytes_received = 0;
                    
                    // Generate filename
                    char filename[64];
                    snprintf(filename, sizeof(filename), "/mnt/data/img_%03d.jpg", m_data_file_count++);
                    m_currentFilename = filename;
                    
                    this->log_ACTIVITY_LO_ImageHeaderReceived();
                    
                    // Remove the IMG_START line from buffer
                    U32 remainingSize = m_protocolBufferSize - (lineEndIndex + 1);
                    if (remainingSize > 0) {
                        memmove(m_protocolBuffer, 
                               &m_protocolBuffer[lineEndIndex + 1], 
                               remainingSize);
                    }
                    m_protocolBufferSize = remainingSize;
                    
                    // Any remaining data in protocol buffer is image data, so we transfer it to the image buffer immediately
                    if (m_protocolBufferSize > 0) {
                        if (!accumulateImageData(m_protocolBuffer, m_protocolBufferSize)) {
                            this->log_WARNING_HI_ImageDataOverflow();
                            deallocateImageBuffer();
                            m_receiving = false;
                        }
                        clearProtocolBuffer();  // Clear now that data is moved
                    }
                    
                    // Exit loop - we're now in image receiving mode
                    break;
                } else {
                    // Buffer allocation failed
                    this->log_WARNING_HI_BufferAllocationFailed(IMAGE_BUFFER_SIZE);
                }
            } else {
                // Log other commands/data for debugging
                for (U32 i = 0; i < lineLength && i < 16; ++i) {
                    this->log_ACTIVITY_LO_ByteReceived(m_protocolBuffer[i]);
                }
                
                // Remove processed line from buffer
                U32 remainingSize = m_protocolBufferSize - (lineEndIndex + 1);
                if (remainingSize > 0) {
                    memmove(m_protocolBuffer, 
                           &m_protocolBuffer[lineEndIndex + 1], 
                           remainingSize);
                }
                m_protocolBufferSize = remainingSize;
            }
        } else {
            break;
        }
    }
}

void PayloadHandler ::clearProtocolBuffer() {
    m_protocolBufferSize = 0;
    memset(m_protocolBuffer, 0, PROTOCOL_BUFFER_SIZE);
}

bool PayloadHandler ::allocateImageBuffer() {
    // Request buffer from BufferManager
    m_imageBuffer = this->allocate_out(0, IMAGE_BUFFER_SIZE);

    // Check if allocation succeeded
    if (!m_imageBuffer.isValid() || m_imageBuffer.getSize() < IMAGE_BUFFER_SIZE) {
        this->log_WARNING_HI_BufferAllocationFailed(IMAGE_BUFFER_SIZE);
        deallocateImageBuffer();
        return false;
    }

    m_imageBufferUsed = 0;
    return true;
}

void PayloadHandler ::deallocateImageBuffer() {
    if (m_imageBuffer.isValid()) {
        this->deallocate_out(0, m_imageBuffer);
        m_imageBuffer = Fw::Buffer();  // Reset to invalid buffer
    }
    m_imageBufferUsed = 0;
}

bool PayloadHandler ::accumulateImageData(const U8* data, U32 size) {
    FW_ASSERT(m_imageBuffer.isValid());

    // Check if we have space
    if (m_imageBufferUsed + size > m_imageBuffer.getSize()) {
        return false;
    }

    // Copy data into image buffer
    memcpy(&m_imageBuffer.getData()[m_imageBufferUsed], data, size);
    m_imageBufferUsed += size;
    m_bytes_received += size;

    return true;
}

void PayloadHandler ::processCompleteImage() {
    FW_ASSERT(m_imageBuffer.isValid());

    // Write image to file
    Os::File::Status status = m_file.open(m_currentFilename.c_str(), Os::File::OPEN_WRITE);
    
    if (status == Os::File::OP_OK) {
        // Os::File::write expects FwSizeType& for size parameter
        FwSizeType sizeToWrite = static_cast<FwSizeType>(m_imageBufferUsed);
        status = m_file.write(m_imageBuffer.getData(), sizeToWrite, Os::File::WaitType::NO_WAIT);
        m_file.close();
        
        if (status == Os::File::OP_OK) {
            // Success! sizeToWrite now contains actual bytes written
            Fw::LogStringArg pathArg(m_currentFilename.c_str());
            this->log_ACTIVITY_HI_DataReceived(m_imageBufferUsed, pathArg);
        } else {
            // TODO - log write error
        }
    } else {
        // TODO - log open error
    }

    // Clean up
    deallocateImageBuffer();
    m_receiving = false;
    m_bytes_received = 0;
}

I32 PayloadHandler ::findImageEndMarker(const U8* data, U32 size) {
    // Looking for "\n<IMG_END>" or "<IMG_END>"
    const char* marker = "<IMG_END>";
    const U32 markerLen = 9;  // strlen("<IMG_END>")
    
    if (size < markerLen) {
        return -1;
    }
    
    // Search for the marker
    for (U32 i = 0; i <= size - markerLen; ++i) {
        bool found = true;
        for (U32 j = 0; j < markerLen; ++j) {
            if (data[i + j] != static_cast<U8>(marker[j])) {
                found = false;
                break;
            }
        }
        if (found) {
            // Found marker at position i
            // If preceded by newline, back up to before newline
            if (i > 0 && data[i - 1] == '\n') {
                return static_cast<I32>(i - 1);
            }
            return static_cast<I32>(i);
        }
    }
    
    return -1;  // Not found
}

bool PayloadHandler ::isImageStartCommand(const U8* line, U32 length) {
    const char* command = "<IMG_START>";
    const U32 cmdLen = 11;  // strlen("<IMG_START>")
    
    if (length != cmdLen) {
        return false;
    }
    
    for (U32 i = 0; i < cmdLen; ++i) {
        if (line[i] != static_cast<U8>(command[i])) {
            return false;
        }
    }
    
    return true;
}

}  // namespace Components
