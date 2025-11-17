// ======================================================================
// \title  CameraHandler.hpp
// \author moises
// \brief  hpp file for CameraHandler component implementation class
// ======================================================================

#ifndef Components_CameraHandler_HPP
#define Components_CameraHandler_HPP

#include <string>
#include <cstddef>
#include "FprimeZephyrReference/Components/CameraHandler/CameraHandlerComponentAc.hpp"
#include "Os/File.hpp">

namespace Components {

class CameraHandler final : public CameraHandlerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct CameraHandler object
    CameraHandler(const char* const compName  //!< The component name
    );

    //! Destroy CameraHandler object
    ~CameraHandler();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! Receives data from PayloadCom over UART, handles image file saving logic
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& buffer,
                        const Drv::ByteStreamStatus& status) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TAKE_IMAGE
    //!
    //! Type in "snap" to capture an image
    void TAKE_IMAGE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq            //!< The command sequence number
                               ) override;

    //! Handler implementation for command SEND_COMMAND
    void SEND_COMMAND_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                 U32 cmdSeq,           //!< The command sequence number
                                 const Fw::CmdStringArg& cmd) override;

    U8 m_data_file_count = 0;
    bool m_receiving = false; 
    U32 m_bytes_received = 0;

    U8 m_lineBuffer[128];
    size_t m_lineIndex = 0;
    Os::File m_file;
    std::string m_currentFilename;
    bool m_fileOpen = false;  // Track if file is currently open for writing

    // Small protocol buffer for commands/headers (static allocation)
    static constexpr U32 PROTOCOL_BUFFER_SIZE = 128;  // Just enough for header
    U8 m_protocolBuffer[PROTOCOL_BUFFER_SIZE];
    U32 m_protocolBufferSize = 0;
    
    // Protocol constants for image transfer
    // Protocol: <IMG_START><SIZE>[4-byte uint32]</SIZE>[image data]<IMG_END>
    static constexpr U32 IMG_START_LEN = 11;     // strlen("<IMG_START>")
    static constexpr U32 SIZE_TAG_LEN = 6;       // strlen("<SIZE>")
    static constexpr U32 SIZE_VALUE_LEN = 4;     // 4-byte little-endian uint32
    static constexpr U32 SIZE_CLOSE_TAG_LEN = 7; // strlen("</SIZE>")
    static constexpr U32 IMG_END_LEN = 9;        // strlen("<IMG_END>")
    
    // Derived constants
    static constexpr U32 HEADER_SIZE = IMG_START_LEN + SIZE_TAG_LEN + SIZE_VALUE_LEN + SIZE_CLOSE_TAG_LEN;  // 28 bytes
    static constexpr U32 SIZE_TAG_OFFSET = IMG_START_LEN;                    // 11
    static constexpr U32 SIZE_VALUE_OFFSET = IMG_START_LEN + SIZE_TAG_LEN;   // 17
    static constexpr U32 SIZE_CLOSE_TAG_OFFSET = SIZE_VALUE_OFFSET + SIZE_VALUE_LEN;  // 21
    
    U32 m_expected_size = 0;  // Expected image size from header
};

}  // namespace Components

#endif
