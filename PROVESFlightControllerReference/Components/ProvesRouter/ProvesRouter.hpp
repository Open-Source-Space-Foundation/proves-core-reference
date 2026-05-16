// ======================================================================
// \title  ProvesRouter.hpp
// \author Ines (based on thomas-bc's FprimeRouter.hpp)
// \brief  hpp file for ProvesRouter component implementation class
// ======================================================================

#ifndef Svc_ProvesRouter_HPP
#define Svc_ProvesRouter_HPP

#include "PROVESFlightControllerReference/Components/ProvesRouter/ProvesRouterComponentAc.hpp"

namespace Svc {

class ProvesRouter final : public ProvesRouterComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ProvesRouter object
    ProvesRouter(const char* const compName  //!< The component name
    );

    //! Destroy ProvesRouter object
    ~ProvesRouter();

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
    // ! This is a no-op because ProvesRouter does not need to handle command responses
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
