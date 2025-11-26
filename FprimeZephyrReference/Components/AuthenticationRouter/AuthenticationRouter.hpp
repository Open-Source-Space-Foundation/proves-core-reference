// ======================================================================
// \title  AuthenticationRouter.hpp
// \author Ines (based on thomas-bc's FprimeRouter.hpp)
// \brief  hpp file for AuthenticationRouter component implementation class
// ======================================================================

#ifndef Svc_AuthenticationRouter_HPP
#define Svc_AuthenticationRouter_HPP

#include "FprimeZephyrReference/Components/AuthenticationRouter/AuthenticationRouterComponentAc.hpp"

namespace Svc {

class AuthenticationRouter final : public AuthenticationRouterComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct AuthenticationRouter object
    AuthenticationRouter(const char* const compName  //!< The component name
    );

    //! Destroy AuthenticationRouter object
    ~AuthenticationRouter();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for bufferIn
    //! Receiving Fw::Buffer from Deframer
    void dataIn_handler(FwIndexType portNum,                 //!< The port number
                        Fw::Buffer& packetBuffer,            //!< The packet buffer
                        const ComCfg::FrameContext& context  //!< The context object
                        ) override;

    // ! Handler for input port cmdResponseIn
    // ! This is a no-op because AuthenticationRouter does not need to handle command responses
    // ! but the port must be connected
    void cmdResponseIn_handler(FwIndexType portNum,             //!< The port number
                               FwOpcodeType opcode,             //!< The command opcode
                               U32 cmdSeq,                      //!< The command sequence number
                               const Fw::CmdResponse& response  //!< The command response
                               ) override;

    //! Handler implementation for fileBufferReturnIn
    //!
    //! Port for receiving ownership back of buffers sent on fileOut
    void fileBufferReturnIn_handler(FwIndexType portNum,  //!< The port number
                                    Fw::Buffer& fwBuffer  //!< The buffer
                                    ) override;
};
}  // namespace Svc

#endif
