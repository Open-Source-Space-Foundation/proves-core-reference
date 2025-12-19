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

    //! Handler implementation for run port
    //! Port receiving calls from the rate group for periodic command loss time checking
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    //! Checks whether or not the opcode of the packet is in the list of
    //! opcodes that bypassauthentification
    bool BypassesAuthentification(Fw::Buffer& packetBuffer);

    //! Calls safemode when command loss time expires
    void CallSafeMode();

    //! Updates the command loss start time
    //! @param write_to_file If true, writes current time to file and returns it. If false, reads from file.
    //! @return The command loss start time (current time if writing, stored time if reading)
    Fw::Time update_command_loss_start(bool write_to_file = false);

    //! Flag to track if safe mode has been called for the current command loss event
    bool m_safeModeCalled;

    //! Cached command loss start time (initialized to zero, loaded from file on first read)
    Fw::Time m_commandLossStartTime;
};

}  // namespace Svc

#endif
