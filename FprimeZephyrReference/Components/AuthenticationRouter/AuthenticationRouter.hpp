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

    //! Reads the command loss expired flag from file
    //!
    //! Returns the flag value from file, or false if file doesn't exist
    bool readCommandLossExpiredFlag();

    //! Writes the command loss expired flag to file
    //!
    //! Persists the flag value to file
    void writeCommandLossExpiredFlag(bool flag);

    //! Checks whether or not the opcode of the packet is in the list of
    //! opcodes that bypassauthentification
    bool BypassesAuthentification(Fw::Buffer& packetBuffer);

    Fw::On m_state_rtc = Fw::On::OFF;           // keeps track if the RTC is on or if we are using Zephyr's time
    std::atomic<U32> m_commandLossTimeCounter;  // makes this an atomic variable (so its set only in one command),

    bool m_TypeTimeFlag;                          //!< Flag to indicate if the time is RTC or monotonic
    bool m_previousTypeTimeFlag;                  //!< Flag to track previous time type to detect switches
    bool m_commandLossTimeExpiredLogged = false;  //!< Flag to indicate if the command loss time has expired, false by
                                                  //!< default meaning no command loss time has expired yet
    // if the file system fails this will prevent it from being stuck in a boot loop. Need to ensure false is written to
    // the file when the system by default
};

}  // namespace Svc

#endif
