// ======================================================================
// \title  PayloadCom.hpp
// \author robertpendergrast, moisesmata
// \brief  hpp file for PayloadCom component implementation class
// ======================================================================

#ifndef FprimeZephyrReference_PayloadCom_HPP
#define FprimeZephyrReference_PayloadCom_HPP

#include <string>
#include <cstddef>
#include "Os/File.hpp"
#include "FprimeZephyrReference/Components/PayloadCom/PayloadComComponentAc.hpp"

namespace Components {

class PayloadCom final : public PayloadComComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PayloadCom object
    PayloadCom(const char* const compName  //!< The component name
    );

    //! Destroy PayloadCom object
    ~PayloadCom();


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

    //! Write data chunk directly to open file
    //! Returns true on success
    bool writeChunkToFile(const U8* data, U32 size);

    //! Close file and finalize image transfer
    void finalizeImageTransfer();

    //! Handle file write error
    void handleFileError();

    //! Check if buffer contains image end marker
    //! Returns position of marker start, or -1 if not found
    I32 findImageEndMarker(const U8* data, U32 size);

    //! Parse line for image start command
    //! Returns true if line is "<IMG_START>"
    bool isImageStartCommand(const U8* line, U32 length);

    //! Send acknowledgment over UART
    void sendAck();
};

}  // namespace Components

#endif
