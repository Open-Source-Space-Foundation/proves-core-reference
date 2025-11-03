// ======================================================================
// \title  PayloadHandler.cpp
// \author robertpendergrast
// \brief  cpp file for PayloadHandler component implementation class
// ======================================================================
#include "Os/File.hpp"
#include "FprimeZephyrReference/Components/PayloadHandler/PayloadHandler.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PayloadHandler ::PayloadHandler(const char* const compName) : PayloadHandlerComponentBase(compName) {}
PayloadHandler ::~PayloadHandler() {}


// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------


void PayloadHandler ::in_port_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {

    const U8* data = buffer.getData();
    FwSizeType size = buffer.getSize();

    for (FwSizeType i = 0; i < size; i++) {
        // Process each byte of data as needed
        U8 byte = data[i];


        if (!m_receiving){
            // We are not currently receiving a file

            // Append byte to line buffer: This is how we check the header to determine data type
            if (m_lineIndex < sizeof(m_lineBuffer) - 1) {
                m_lineBuffer[m_lineIndex++] = byte;
            }

            // Have we reached the end of the line? If so that means we have a header
            // Check to see what the header is. 
            if (byte == '\n' || byte == '\r') {
                m_lineBuffer[m_lineIndex] = 0; // Null-terminate
                m_lineIndex = 0;

                // Check the header.
                // Right now I'm just checking for an image start tag, but we can expand this to other types later
                if (strstr((const char*)m_lineBuffer, "<IMG_START>")) {
                    m_receiving = true;
                    m_bytes_received = 0;
                    m_expected_size = 0;
                    continue; 
                }

                // If in receiving mode and expected size not set, this line is the size
                if (m_receiving && m_expected_size == 0) {

                    // First we set the expected size
                    m_expected_size = atoi((const char*)m_lineBuffer);


                    // Then we open the file to write to, which we will be writing to over a lot of iterations
                    if (m_data_file_count >= 9) {
                        m_data_file_count = 0;
                    }

                    char filenameBuffer[20];
                    snprintf(filenameBuffer, sizeof(filenameBuffer), "payload_%d.jpg", m_data_file_count);
                    m_currentFilename = filenameBuffer;

                    // Open the file and prepare to write in the next iteration
                    Os::File::Status fileStatus = m_file.open(m_currentFilename.c_str(), Os::File::OPEN_CREATE, Os::File::OVERWRITE);
                    if (fileStatus != Os::File::OP_OK) {
                        m_receiving = false;
                        continue;
                    }
                    continue;
                }
            }


        } else if (m_bytes_received < m_expected_size){
            // We are currently receiving a file

            // Cast byte to a buffer
            // Write a byte to the file
            FwSizeType oneByte = 1;
            m_file.write(&byte, oneByte);
            m_bytes_received++;

            // Check to see if we are done receiving
            if (m_bytes_received >= m_expected_size){
                m_file.flush();
                m_file.close();
                m_receiving = false;   
                m_data_file_count++;

                // Log data received event
                Fw::LogStringArg logPath(m_currentFilename.c_str());
                this->log_ACTIVITY_HI_DataReceived(m_bytes_received, logPath);
            }
        }
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

}  // namespace Components
