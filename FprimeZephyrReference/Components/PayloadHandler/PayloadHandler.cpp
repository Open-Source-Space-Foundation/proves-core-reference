// ======================================================================
// \title  PayloadHandler.cpp
// \author robertpendergrast, moisesmata
// \brief  cpp file for PayloadHandler component implementation class
// ======================================================================
#include "Os/File.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "FprimeZephyrReference/Components/PayloadHandler/PayloadHandler.hpp"
#include <cstring>
#include <cstdio>

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PayloadHandler ::PayloadHandler(const char* const compName)
    : PayloadHandlerComponentBase(compName),
      m_protocolBufferSize(0),
      m_fileOpen(false) {
    // Initialize protocol buffer to zero
    memset(m_protocolBuffer, 0, PROTOCOL_BUFFER_SIZE);
}

PayloadHandler ::~PayloadHandler() {
    // Close file if still open
    if (m_fileOpen) {
        m_file.close();
        m_fileOpen = false;
    }
}


// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------


void PayloadHandler ::in_port_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {

    this->log_ACTIVITY_LO_UartReceived();

    // Check if we received data successfully
    if (status != Drv::ByteStreamStatus::OP_OK) {
        // Log error and abort if receiving
        if (m_receiving && m_fileOpen) {
            handleFileError();
        }
        // CRITICAL: Must return buffer even on error to prevent leak
        if (buffer.isValid()) {
            this->bufferReturn_out(0, buffer);
        }
        return;
    }

    // Check if buffer is valid
    if (!buffer.isValid()) {
        // Buffer is invalid but status was OK - unusual case, just return
        return;
    }

    // Get the data from the UART driver's buffer (we don't own it, just read it)
    const U8* data = buffer.getData();
    U32 dataSize = static_cast<U32>(buffer.getSize());

    // DEBUG: Log first 8 bytes ONLY if not receiving (reduces load during transfer)
    if (!m_receiving && dataSize >= 8) {
        this->log_ACTIVITY_LO_RawDataDump(
            data[0], data[1], data[2], data[3],
            data[4], data[5], data[6], data[7]
        );
    }

    if (m_receiving && m_fileOpen) {
        // Currently receiving image data - write directly to file
        
        // Calculate how much to write (don't exceed expected size)
        U32 remaining = m_expected_size - m_bytes_received;
        U32 toWrite = (dataSize < remaining) ? dataSize : remaining;
        
        // Write chunk to file
        if (!writeChunkToFile(data, toWrite)) {
            // Write failed
            this->log_WARNING_HI_CommandError(Fw::LogStringArg("File write failed"));
            handleFileError();
            // CRITICAL: Return buffer even on error
            this->bufferReturn_out(0, buffer);
            return;
        }
        
        m_bytes_received += toWrite;
        
        // Send ACK after successfully writing chunk
        sendAck();
        
        // Log progress less frequently to reduce system load (every 512 bytes)
        if ((m_bytes_received % 512) < 64) {
            this->log_ACTIVITY_LO_ImageTransferProgress(m_bytes_received, m_expected_size);
        }
        
        // Check if we've received all expected data
        if (m_bytes_received >= m_expected_size) {
            // Image is complete!
            finalizeImageTransfer();
            
            // If there's extra data after the image (e.g., <IMG_END> or next header),
            // push it to protocol buffer
            U32 extraBytes = dataSize - toWrite;
            if (extraBytes > 0) {
                const U8* extraData = data + toWrite;
                if (accumulateProtocolData(extraData, extraBytes)) {
                    processProtocolBuffer();
                }
            }
        }
    } else {
        // Not receiving image - accumulate protocol data
        
        // If protocol buffer is getting too full (> 90%), clear old data
        // This prevents overflow from text responses that aren't image headers
        if (m_protocolBufferSize > (PROTOCOL_BUFFER_SIZE * 9 / 10)) {
            // Keep only last 32 bytes in case header is split
            if (m_protocolBufferSize > 32) {
                memmove(m_protocolBuffer, &m_protocolBuffer[m_protocolBufferSize - 32], 32);
                m_protocolBufferSize = 32;
            }
        }
        
        if (!accumulateProtocolData(data, dataSize)) {
            // Protocol buffer overflow - clear old data and keep new
            clearProtocolBuffer();
            // Try again with cleared buffer
            if (!accumulateProtocolData(data, dataSize)) {
                // Still won't fit - just take what we can
                U32 canFit = PROTOCOL_BUFFER_SIZE;
                memcpy(m_protocolBuffer, data, canFit);
                m_protocolBufferSize = canFit;
            }
        }

        // Log what we have in protocol buffer for debugging
        if (m_protocolBufferSize > 0) {
            this->log_ACTIVITY_LO_ProtocolBufferDebug(m_protocolBufferSize, m_protocolBuffer[0]);
        }

        // Process protocol buffer to detect image headers/commands
        processProtocolBuffer();
    }
    
    // CRITICAL: Return buffer to driver so it can deallocate to BufferManager
    // This matches the ComStub pattern: driver allocates, handler processes, handler returns
    this->bufferReturn_out(0, buffer);
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
    // Protocol: <IMG_START><SIZE>[4-byte little-endian uint32]</SIZE>[image data]<IMG_END>
    
    // Log parse attempt for debugging
    if (m_protocolBufferSize > 0) {
        this->log_ACTIVITY_LO_HeaderParseAttempt(m_protocolBufferSize);
        
        // Dump first 8 bytes for debugging
        if (m_protocolBufferSize >= 8) {
            this->log_ACTIVITY_LO_RawDataDump(
                m_protocolBuffer[0], m_protocolBuffer[1], m_protocolBuffer[2], m_protocolBuffer[3],
                m_protocolBuffer[4], m_protocolBuffer[5], m_protocolBuffer[6], m_protocolBuffer[7]
            );
        }
    }
    
    // Search for <IMG_START> anywhere in the buffer (not just at position 0)
    I32 headerStart = -1;
    
    // Only search if we have enough bytes for the header marker
    if (m_protocolBufferSize >= IMG_START_LEN) {
        for (U32 i = 0; i <= m_protocolBufferSize - IMG_START_LEN; ++i) {
            if (isImageStartCommand(&m_protocolBuffer[i], m_protocolBufferSize - i)) {
                headerStart = static_cast<I32>(i);
                break;
            }
        }
    }
    
    if (headerStart == -1) {
        // No header found - if buffer is nearly full, discard old data
        // Be aggressive: if buffer is > 50% full and no header, it's probably text responses
        if (m_protocolBufferSize > (PROTOCOL_BUFFER_SIZE / 2)) {
            // Keep last 16 bytes in case header is split across chunks
            if (m_protocolBufferSize > 16) {
                memmove(m_protocolBuffer, &m_protocolBuffer[m_protocolBufferSize - 16], 16);
                m_protocolBufferSize = 16;
            } else {
                // Buffer is small enough, just clear it
                clearProtocolBuffer();
            }
        }
        return;
    }
    
    // Found header start! Discard everything before it
    if (headerStart > 0) {
        U32 remaining = m_protocolBufferSize - static_cast<U32>(headerStart);
        memmove(m_protocolBuffer, &m_protocolBuffer[headerStart], remaining);
        m_protocolBufferSize = remaining;
        
        // Log that we found and moved the header
        this->log_ACTIVITY_LO_ProtocolBufferDebug(m_protocolBufferSize, m_protocolBuffer[0]);
    }
    
    // Now check if we have the complete header (28 bytes minimum)
    if (m_protocolBufferSize < HEADER_SIZE) {
        // Not enough data yet, wait for more
        return;
    }
    
    // Check if we have the complete header
    if (m_protocolBufferSize >= HEADER_SIZE) {
        // Check for <IMG_START> at the beginning
        if (!isImageStartCommand(m_protocolBuffer, m_protocolBufferSize)) {
            return;  // Not an image header
        }
        
        // Check for <SIZE> tag after <IMG_START>
        const char* sizeTag = "<SIZE>";
        bool hasSizeTag = true;
        
        for (U32 i = 0; i < SIZE_TAG_LEN; ++i) {
            if (m_protocolBuffer[SIZE_TAG_OFFSET + i] != static_cast<U8>(sizeTag[i])) {
                hasSizeTag = false;
                break;
            }
        }
        
        if (!hasSizeTag) {
            return;  // Invalid header
        }
        
        // Extract 4-byte size (little-endian)
        U32 imageSize = 0;
        imageSize |= static_cast<U32>(m_protocolBuffer[SIZE_VALUE_OFFSET + 0]);
        imageSize |= static_cast<U32>(m_protocolBuffer[SIZE_VALUE_OFFSET + 1]) << 8;
        imageSize |= static_cast<U32>(m_protocolBuffer[SIZE_VALUE_OFFSET + 2]) << 16;
        imageSize |= static_cast<U32>(m_protocolBuffer[SIZE_VALUE_OFFSET + 3]) << 24;
        
        // Verify </SIZE> tag
        const char* closeSizeTag = "</SIZE>";
        bool hasCloseSizeTag = true;
        
        for (U32 i = 0; i < SIZE_CLOSE_TAG_LEN; ++i) {
            if (m_protocolBuffer[SIZE_CLOSE_TAG_OFFSET + i] != static_cast<U8>(closeSizeTag[i])) {
                hasCloseSizeTag = false;
                break;
            }
        }
        
        if (!hasCloseSizeTag) {
            return;  // Invalid header
        }
        
        // Valid header! Open file immediately for streaming
        m_receiving = true;
        m_bytes_received = 0;
        m_expected_size = imageSize;
        
        // Log the extracted size
        this->log_ACTIVITY_HI_ImageSizeExtracted(imageSize);
        
        // Generate filename - save to root filesystem
        char filename[64];
        snprintf(filename, sizeof(filename), "/img_%03d.jpg", m_data_file_count++);
        m_currentFilename = filename;
        
        // Open file for writing
        Os::File::Status status = m_file.open(m_currentFilename.c_str(), Os::File::OPEN_WRITE);
        
        if (status != Os::File::OP_OK) {
            // Failed to open file
            this->log_WARNING_HI_CommandError(Fw::LogStringArg("Failed to open file"));
            m_receiving = false;
            m_expected_size = 0;
            clearProtocolBuffer();
            return;
        }
        
        m_fileOpen = true;
        this->log_ACTIVITY_LO_ImageHeaderReceived();

        // Send ACK to camera after successfully parsing header and opening file
        sendAck();
        
        // Remove header from protocol buffer
        U32 remainingSize = m_protocolBufferSize - HEADER_SIZE;
        if (remainingSize > 0) {
            memmove(m_protocolBuffer, 
                   &m_protocolBuffer[HEADER_SIZE], 
                   remainingSize);
        }
        m_protocolBufferSize = remainingSize;
        
        // Write any remaining data (image data) directly to file
        // NOTE: This should be empty since camera waits for ACK before sending data
        if (m_protocolBufferSize > 0) {
            U32 toWrite = (m_protocolBufferSize < m_expected_size) ? m_protocolBufferSize : m_expected_size;
            
            if (writeChunkToFile(m_protocolBuffer, toWrite)) {
                m_bytes_received += toWrite;
                
                // Check if complete already
                if (m_bytes_received >= m_expected_size) {
                    finalizeImageTransfer();
                }
            } else {
                handleFileError();
            }
            
            clearProtocolBuffer();
        }
    }
}

void PayloadHandler ::clearProtocolBuffer() {
    m_protocolBufferSize = 0;
    memset(m_protocolBuffer, 0, PROTOCOL_BUFFER_SIZE);
}

bool PayloadHandler ::writeChunkToFile(const U8* data, U32 size) {
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
    
    return true;
}

void PayloadHandler ::finalizeImageTransfer() {
    if (!m_fileOpen) {
        return;
    }

    // Close the file
    m_file.close();
    m_fileOpen = false;

    // Log success
    Fw::LogStringArg pathArg(m_currentFilename.c_str());
    this->log_ACTIVITY_HI_DataReceived(m_bytes_received, pathArg);
    
    // Send final ACK to camera to confirm complete transfer
    sendAck();

    // Reset state
    m_receiving = false;
    m_bytes_received = 0;
    m_expected_size = 0;
}

void PayloadHandler ::handleFileError() {
    // Close file if open
    if (m_fileOpen) {
        m_file.close();
        m_fileOpen = false;
    }

    // Log error
    this->log_WARNING_HI_CommandError(Fw::LogStringArg("File write error"));

    // Reset state
    m_receiving = false;
    m_bytes_received = 0;
    m_expected_size = 0;
    clearProtocolBuffer();
}

I32 PayloadHandler ::findImageEndMarker(const U8* data, U32 size) {
    // Looking for "\n<IMG_END>" or "<IMG_END>"
    const char* marker = "<IMG_END>";
    
    if (size < IMG_END_LEN) {
        return -1;
    }
    
    // Search for the marker
    for (U32 i = 0; i <= size - IMG_END_LEN; ++i) {
        bool found = true;
        for (U32 j = 0; j < IMG_END_LEN; ++j) {
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
    
    if (length < IMG_START_LEN) {
        return false;
    }
    
    for (U32 i = 0; i < IMG_START_LEN; ++i) {
        if (line[i] != static_cast<U8>(command[i])) {
            return false;
        }
    }
    
    return true;
}

void PayloadHandler ::sendAck(){
    // Send an acknowledgment over UART
    const char* ackMsg = "<MOISES>\n";
    Fw::Buffer ackBuffer(
        reinterpret_cast<U8*>(const_cast<char*>(ackMsg)), 
        strlen(ackMsg)
    );
    this->out_port_out(0, ackBuffer);

}

}  // namespace Components
