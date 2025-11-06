// ======================================================================
// \title  PayloadHandler.hpp
// \author robertpendergrast
// \brief  hpp file for PayloadHandler component implementation class
// ======================================================================

#ifndef FprimeZephyrReference_PayloadHandler_HPP
#define FprimeZephyrReference_PayloadHandler_HPP

#include <string>
#include <cstddef>
#include "Os/File.hpp"
#include "FprimeZephyrReference/Components/PayloadHandler/PayloadHandlerComponentAc.hpp"

namespace Components {

class PayloadHandler final : public PayloadHandlerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PayloadHandler object
    PayloadHandler(const char* const compName  //!< The component name
    );

    //! Destroy PayloadHandler object
    ~PayloadHandler();

    U8 m_data_file_count = 0;
    bool m_receiving = false; 
    U32 m_bytes_received = 0;

    U8 m_lineBuffer[128];
    size_t m_lineIndex = 0;
    Os::File m_file;
    std::string m_currentFilename;

    // Small protocol buffer for commands/headers (static allocation)
    static constexpr U32 PROTOCOL_BUFFER_SIZE = 2048;
    U8 m_protocolBuffer[PROTOCOL_BUFFER_SIZE];
    U32 m_protocolBufferSize = 0;

    // Large image buffer (dynamic allocation via BufferManager)
    Fw::Buffer m_imageBuffer;
    U32 m_imageBufferUsed = 0;  // Bytes used in image buffer
    static constexpr U32 IMAGE_BUFFER_SIZE = 256 * 1024;  // 256 KB for images

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for in_port
     //! Handler implementation for in_port
    void in_port_handler(FwIndexType portNum,  //!< The port number
                         Fw::Buffer& buffer,
                         const Drv::ByteStreamStatus& status);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SEND_COMMAND
    //!
    //! TODO
    void SEND_COMMAND_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                 U32 cmdSeq,           //!< The command sequence number
                                 const Fw::CmdStringArg& cmd);

    // ----------------------------------------------------------------------
    // Helper methods for data accumulation
    // ----------------------------------------------------------------------

    //! Accumulate protocol data (headers, commands)
    //! Returns true if data was successfully accumulated, false on overflow
    bool accumulateProtocolData(const U8* data, U32 size);

    //! Process protocol buffer to detect commands/image headers
    void processProtocolBuffer();

    //! Clear the protocol buffer
    void clearProtocolBuffer();

    //! Allocate image buffer from BufferManager
    //! Returns true on success
    bool allocateImageBuffer();

    //! Deallocate image buffer
    void deallocateImageBuffer();

    //! Accumulate image data into dynamically allocated buffer
    //! Returns true on success, false on overflow
    bool accumulateImageData(const U8* data, U32 size);

    //! Process complete image (write to file, send event, etc.)
    void processCompleteImage();

    //! Check if buffer contains image end marker
    //! Returns position of marker start, or -1 if not found
    I32 findImageEndMarker(const U8* data, U32 size);

    //! Parse line for image start command
    //! Returns true if line is "<IMG_START>"
    bool isImageStartCommand(const U8* line, U32 length);
};

}  // namespace Components

#endif
