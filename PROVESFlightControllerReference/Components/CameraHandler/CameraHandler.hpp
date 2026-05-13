// ======================================================================
// \title  CameraHandler.hpp
// \author moises
// \brief  hpp file for CameraHandler component implementation class
//         Handles camera protocol processing and image file saving
// ======================================================================

#ifndef Components_CameraHandler_HPP
#define Components_CameraHandler_HPP

#include <cstddef>
#include <string>

#include "Os/File.hpp"
#include "PROVESFlightControllerReference/Components/CameraHandler/CameraHandlerComponentAc.hpp"

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

    void configure(U32 cam_num) { this->cam_number = cam_num; }

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //! Receives data from PayloadCom, handles image protocol parsing and file saving
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& buffer,
                        const Drv::ByteStreamStatus& status) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TAKE_IMAGE
    //! Type in "snap" to capture an image
    void TAKE_IMAGE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq            //!< The command sequence number
                               ) override;

    //! Handler implementation for command SEND_COMMAND
    void SEND_COMMAND_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                 U32 cmdSeq,           //!< The command sequence number
                                 const Fw::CmdStringArg& cmd) override;

    void PING_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                         U32 cmdSeq            //!< The command sequence number
                         ) override;

    // ----------------------------------------------------------------------
    // Helper methods for protocol processing
    // ----------------------------------------------------------------------

    //! Accumulate protocol data (headers, commands)
    //! Returns true if data was successfully accumulated, false on overflow
    bool accumulateProtocolData(const U8* data, U32 size);

    //! Process protocol buffer to detect commands/image headers
    void processProtocolBuffer();

    //! Clear the protocol buffer
    void clearProtocolBuffer();

    //! Write data chunk directly to open file
    //! Returns true on success
    bool writeChunkToFile(const U8* data, U32 size);

    //! Close file and finalize image transfer
    void finalizeImageTransfer();

    //! Handle file write error
    void handleFileError();

    bool writeImageCount(U32 count);
    bool readImageCount(U32& count);

    //! Check if buffer contains image end marker
    //! Returns position of marker start, or -1 if not found
    I32 findImageEndMarker(const U8* data, U32 size);

    //! Parse line for image start command
    //! Returns true if line is "<IMG_START>"
    bool isImageStartCommand(const U8* line, U32 length);

    bool isPong(const U8* line, U32 length);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    U8 m_data_file_count = 0;
    bool m_receiving = false;
    bool m_waiting_for_pong = false;

    U32 m_bytes_received = 0;
    U32 m_file_error_count = 0;  // Track total file errors
    U32 m_images_saved = 0;      // Track total images successfully saved
    U32 cam_number = 0;          // Camera number for filename generation

    U8 m_lineBuffer[128];
    size_t m_lineIndex = 0;
    Os::File m_file;
    std::string m_currentFilename;
    bool m_fileOpen = false;  // Track if file is currently open for writing
    const char* IMAGE_COUNT_PATH = "/image_count.bin";

    // Small protocol buffer for commands/headers (static allocation)
    static constexpr U32 PROTOCOL_BUFFER_SIZE = 128;  // Just enough for header
    U8 m_protocolBuffer[PROTOCOL_BUFFER_SIZE];
    U32 m_protocolBufferSize = 0;

    // Protocol constants for image transfer
    // Protocol: <IMG_START><SIZE>[4-byte uint32]</SIZE>[image data]<IMG_END>
    static constexpr U32 IMG_START_LEN = 11;      // strlen("<IMG_START>")
    static constexpr U32 SIZE_TAG_LEN = 6;        // strlen("<SIZE>")
    static constexpr U32 SIZE_VALUE_LEN = 4;      // 4-byte little-endian uint32
    static constexpr U32 SIZE_CLOSE_TAG_LEN = 7;  // strlen("</SIZE>")
    static constexpr U32 IMG_END_LEN = 9;         // strlen("<IMG_END>")
    static constexpr U32 PONG_LEN = 4;            // strlen("PONG")
    static constexpr U32 QUAL_SET_HD = 22;        // strlen("<FRAME_CHANGE_SUCCESS>")

    // Derived constants
    static constexpr U32 HEADER_SIZE = IMG_START_LEN + SIZE_TAG_LEN + SIZE_VALUE_LEN + SIZE_CLOSE_TAG_LEN;  // 28 bytes
    static constexpr U32 SIZE_TAG_OFFSET = IMG_START_LEN;                                                   // 11
    static constexpr U32 SIZE_VALUE_OFFSET = IMG_START_LEN + SIZE_TAG_LEN;                                  // 17
    static constexpr U32 SIZE_CLOSE_TAG_OFFSET = SIZE_VALUE_OFFSET + SIZE_VALUE_LEN;                        // 21

    U32 m_expected_size = 0;  // Expected image size from header
    U8 m_lastMilestone = 0;   // Last progress milestone emitted (0, 25, 50, 75)
};

}  // namespace Components

#endif
