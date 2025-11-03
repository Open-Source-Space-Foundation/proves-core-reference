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
    U32 m_expected_size = 0;
    U32 m_bytes_received = 0;

    U8 m_lineBuffer[128];
    size_t m_lineIndex = 0;
    Os::File m_file;
    std::string m_currentFilename;

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
};

}  // namespace Components

#endif
