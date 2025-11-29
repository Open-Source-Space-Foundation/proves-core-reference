// ======================================================================
// \title  MosaicHandler.hpp
// \author moises
// \brief  hpp file for MosaicHandler component implementation class
//         Handles camera protocol processing and image file saving
// ======================================================================

#ifndef Components_MosaicHandler_HPP
#define Components_MosaicHandler_HPP

#include <cstddef>
#include <string>

#include "FprimeZephyrReference/Components/MosaicHandler/MosaicHandlerComponentAc.hpp"
#include "Os/File.hpp"

namespace Components {

class MosaicHandler final : public MosaicHandlerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct MosaicHandler object
    MosaicHandler(const char* const compName  //!< The component name
    );

    //! Destroy MosaicHandler object
    ~MosaicHandler();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! Receives data from PayloadCom, handles mosaic protocol parsing and file saving
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& buffer,
                        const Drv::ByteStreamStatus& status) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TAKE_GAMMA_READING
    //!
    //! Type in "gamma_begin" to capture a gamma reading
    void TAKE_GAMMA_READING_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                       U32 cmdSeq            //!< The command sequence number
                                       ) override;

    //! Handler implementation for command SEND_COMMAND
    //!
    //! Send command to mosaic via PayloadCom
    void SEND_COMMAND_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                 U32 cmdSeq,           //!< The command sequence number
                                 const Fw::CmdStringArg& cmd) override;

    // ----------------------------------------------------------------------
    // Helper methods for protocol processing
    // ----------------------------------------------------------------------

    //! Accumulate protocol data (headers, commands)
    //! Returns true if data was successfully accumulated, false on overflow
    bool accumulateProtocolData(const U8* data, U32 size);

    //! Process protocol buffer to detect commands/gamma reading headers
    void processProtocolBuffer();

    //! Clear the protocol buffer
    void clearProtocolBuffer();

    //! Write data chunk directly to open file
    //! Returns true on success
    bool writeChunkToFile(const U8* data, U32 size);

    //! Close file and finalize image transfer
    void finalizeMosaicTransfer();

    //! Handle file write error
    void handleFileError();

    //! Check if buffer contains image end marker
    //! Returns position of marker start, or -1 if not found
    I32 findGammaReadingEndMarker(const U8* data, U32 size);

    //! Parse line for image start command
    //! Returns true if line is "<GAMMA_START>"
    bool isGammaReadingStartCommand(const U8* line, U32 length);

    //! Send acknowledgment through PayloadCom to UART
    void sendAck();

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

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
    // Protocol: <GAMMA_START><SIZE>[4-byte uint32]</SIZE>[gamma reading data]<GAMMA_END>
    static constexpr U32 GAMMA_START_LEN = 13;    // strlen("<GAMMA_START>")
    static constexpr U32 SIZE_TAG_LEN = 6;        // strlen("<SIZE>")
    static constexpr U32 SIZE_VALUE_LEN = 4;      // 4-byte little-endian uint32
    static constexpr U32 SIZE_CLOSE_TAG_LEN = 7;  // strlen("</SIZE>")
    static constexpr U32 GAMMA_END_LEN = 11;      // strlen("<GAMMA_END>")

    // Derived constants
    static constexpr U32 HEADER_SIZE =
        GAMMA_START_LEN + SIZE_TAG_LEN + SIZE_VALUE_LEN + SIZE_CLOSE_TAG_LEN;         // 28 bytes
    static constexpr U32 SIZE_TAG_OFFSET = GAMMA_START_LEN;                           // 11
    static constexpr U32 SIZE_VALUE_OFFSET = GAMMA_START_LEN + SIZE_TAG_LEN;          // 17
    static constexpr U32 SIZE_CLOSE_TAG_OFFSET = SIZE_VALUE_OFFSET + SIZE_VALUE_LEN;  // 21

    U32 m_expected_size = 0;  // Expected image size from header
};

}  // namespace Components

#endif
