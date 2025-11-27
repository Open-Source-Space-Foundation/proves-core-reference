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

    //! Handler implementation for schedIn
    //! Port receiving calls from the rate group
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

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

    //! Handler implementation for initializeFiles
    //!
    //! Port for initializing the files
    U32 initializeFiles(const char* filePath);

    //! Handler implementation for writeToFile
    //!
    //! Writes the time to the file
    U32 writeToFile(const char* filePath, U32 time);

    //! Handler implementation for readFromFile
    //!
    //! Reads the time from the file
    U32 readFromFile(const char* filePath);

    //! Handler implementation for getTimeFromRTC
    //!
    //! Gets the time from the RTC
    U32 getTimeFromRTC();

    Fw::On m_state_rtc = Fw::On::OFF;           // keeps track if the RTC is on or if we are using Zephyr's time
    std::atomic<U32> m_commandLossTimeCounter;  // makes this an atomic variable (so its set only in one command),

    bool m_TypeTimeFlag;  //!< Flag to indicate if the time is RTC or monotonic
};

}  // namespace Svc

#endif
