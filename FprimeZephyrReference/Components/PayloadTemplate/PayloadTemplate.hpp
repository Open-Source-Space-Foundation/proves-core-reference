// ======================================================================
// \title  PayloadTemplate.hpp
// \author robertpendergrast
// \brief  hpp file for PayloadTemplate component implementation class
// ======================================================================

#ifndef Components_PayloadTemplate_HPP
#define Components_PayloadTemplate_HPP

#include "FprimeZephyrReference/Components/PayloadTemplate/PayloadTemplateComponentAc.hpp"

namespace Components {

class PayloadTemplate final : public PayloadTemplateComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PayloadTemplate object
    PayloadTemplate(const char* const compName  //!< The component name
    );

    //! Destroy PayloadTemplate object
    ~PayloadTemplate();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! Receives data from PayloadCom, handler to be implemented for your specific payload
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& buffer,
                        const Drv::ByteStreamStatus& status) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SEND_COMMAND
    //!
    //! Send command to your payload over uart
    void SEND_COMMAND_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                 U32 cmdSeq,           //!< The command sequence number
                                 const Fw::CmdStringArg& cmd) override;
};

}  // namespace Components

#endif
