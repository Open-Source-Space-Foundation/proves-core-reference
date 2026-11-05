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
#include "PROVESFlightControllerReference/Components/CameraHandler/ImageProtocolParser.hpp"

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

    //! React to what the parser found: answer a PONG, or start receiving an image
    void handleProtocolResult(const ImageProtocolParser::Result& result);

    //! Open the image file for a header the parser accepted, and write any image data that came with it
    void startImageTransfer(U32 imageSize);

    //! Write data chunk directly to open file
    //! Returns true on success
    bool writeChunkToFile(const U8* data, U32 size);

    //! Close file and finalize image transfer
    void finalizeImageTransfer();

    //! Handle file write error
    void handleFileError();

    bool writeImageCount(U32 count);
    bool readImageCount(U32& count);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    U8 m_data_file_count = 0;
    bool m_waiting_for_pong = false;

    U32 m_file_error_count = 0;  // Track total file errors
    U32 m_images_saved = 0;      // Track total images successfully saved
    U32 cam_number = 0;          // Camera number for filename generation

    U8 m_lineBuffer[128];
    size_t m_lineIndex = 0;
    Os::File m_file;
    std::string m_currentFilename;
    bool m_fileOpen = false;  // Track if file is currently open for writing
    const char* IMAGE_COUNT_PATH = "/image_count.bin";

    //! Parses the camera protocol and tracks the transfer progress
    ImageProtocolParser m_parser;

    static constexpr U32 QUAL_SET_HD = 22;  // strlen("<FRAME_CHANGE_SUCCESS>")
};

}  // namespace Components

#endif
