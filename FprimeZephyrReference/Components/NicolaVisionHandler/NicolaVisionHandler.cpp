// ======================================================================
// \title  NicolaVisionHandler.cpp
// \author wisaac
// \brief  cpp file for NicolaVisionHandler component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/NicolaVisionHandler/NicolaVisionHandler.hpp"

namespace Components {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

NicolaVisionHandler ::NicolaVisionHandler(const char* const compName) : NicolaVisionHandlerComponentBase(compName) {
    
}

NicolaVisionHandler ::~NicolaVisionHandler() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void NicolaVisionHandler ::in_port_handler(FwIndexType portNum,
                                           Fw::Buffer& buffer,
                                           const Drv::ByteStreamStatus& status) {

// Look for magic bytes in the incoming buffer
const U8* buf_data = buffer.getData();
FwSizeType buf_size = buffer.getSize();

for(FwSizeType i = 0; i < buf_size; ++i) {
    if (!this->found_magic) {
        // Try to match magic_bytes sequence
        if (buf_data[i] == static_cast<const U8>(this->magic_bytes[this->magic_pos])) {
            ++magic_pos;
            if (this->magic_pos == sizeof(this->magic_bytes)) {
                this->found_magic = true;
                this->magic_pos = 0;
                // Next 4 bytes after magic header are the image size in little-endian
                if (i + 4 < buf_size) {
                    this->image_len = buf_data[i+1] + (buf_data[i+2]<<8) + (buf_data[i+3]<<16) + (buf_data[i+4]<<24);
                    this->image_received = 0;
                    // Allocate a new buffer
                    this->pic_in_buffer = this->allocate_out(0, this->image_len);
                    if (!this->pic_in_buffer.getData()) {
                        // Buffer allocation failed
                        this->log_WARNING_HI_BufferAllocationFailed();
                        this->found_magic = false;
                        break;
                    }
                    // Start copying image data (if any in this buffer chunk)
                    FwSizeType bytes_from_here = buf_size - (i + 5);
                    if (bytes_from_here > 0) {
                        FwSizeType to_copy = (bytes_from_here > this->image_len) ? this->image_len : bytes_from_here;
                        memcpy(this->pic_in_buffer.getData(), &buf_data[i+5], to_copy);
                        this->image_received += to_copy;
                    }
                    i += 4 + bytes_from_here; // skip size and copied image data
                }
                else {
                    // reading in as many bytes of size as possible, and then wait for next buffer
                    this->szremain = 4 - (buf_size - i - 1);

                    this->image_len = 0;
                    for(FwSizeType j = 0; j < 4- this->szremain; ++j) {
                        this->image_len += buf_data[i+1+j] << (j*8);
                    }
                    break;
                }
            }
            // else: still matching magic bytes
        }
        else {
            magic_pos = 0; // unmatched, restart search
        }
    }
    else if (this->found_magic && this->szremain > 0) {
        this->szremain -= 1;
        this->image_len += buf_data[i] << ((4-this->szremain)*8);
    }
    else if (this->found_magic && this->image_received < this->image_len) {
        // Receiving the image data stream
        FwSizeType remain = this->image_len - this->image_received;
        FwSizeType chunk = ((buf_size - i) > remain) ? remain : (buf_size - i);
        memcpy(reinterpret_cast<U8*>(this->pic_in_buffer.getData()) + this->image_received, buf_data + i, chunk);
        this->image_received += chunk;
        i += chunk - 1;
        if (this->image_received == this->image_len) {
            // Got the full image - call handler or do something with img_buffer
            this->pic_in_buffer = this->pic_in_buffer; // or whatever processing is needed
            // Image fully received, reset state
            this->found_magic = false;
            this->image_len = 0;
            this->image_received = 0;
            // Optionally signal picture completion
            this->log_ACTIVITY_HI_PictureReceived();
            // Hand off buffer as needed, e.g. call another port, etc.
        }
    }
}
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void NicolaVisionHandler ::TakePicture_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Prepare the "snap" command to send over UART via out_port
    Fw::Buffer snapBuffer(0, reinterpret_cast<U64>(snap_cmd), sizeof(snap_cmd) - 1); // exclude null terminator
    // Send the buffer via out_port, check/send status if needed
    Drv::ByteStreamStatus sendStatus = this->out_port_out(0, snapBuffer);
    if (sendStatus != Drv::ByteStreamStatus::OP_OK) {
        this->log_WARNING_HI_TakePictureError();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    else {
        this->log_ACTIVITY_HI_PictureTaken();
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    
}

}  // namespace Components
