// ======================================================================
// \title  Authenticate.hpp
// \brief  hpp file for Authenticate component implementation class
// ======================================================================

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Types/String.hpp>
#include <Os/File.hpp>
#include <atomic>
#include <cassert>

#include "PROVESFlightControllerReference/Components/Authenticate/AuthenticateComponentAc.hpp"

namespace Components {

class Authenticate final : public AuthenticateComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Authenticate object
    Authenticate(const char* const compName  //!< The component name
    );

    //! Destroy Authenticate object
    ~Authenticate();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! Port receiving Space Packets from TcDeframer
    void dataIn_handler(FwIndexType portNum,                 //!< The port number
                        Fw::Buffer& data,                    //!< The buffer containing the packet data
                        const ComCfg::FrameContext& context  //!< The frame context associated with the packet
                        ) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Port receiving back ownership of buffers sent to dataOut
    void dataReturnIn_handler(FwIndexType portNum,                 //!< The port number
                              Fw::Buffer& data,                    //!< The buffer being returned
                              const ComCfg::FrameContext& context  //!< The frame context associated with the buffer
                              ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command GET_SEQ_NUM
    void GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq            //!< The command sequence number
                                ) override;

    //! Handler implementation for command SET_SEQ_NUM
    void SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq,           //!< The command sequence number
                                U32 seqNum            //!< The sequence number to set
                                ) override;

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Initialize component
    //!
    //! Loads the sequence number from persistent storage
    void init(FwEnumStoreType instance  //!< The instance number
    );

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    // Loads the sequence number from the specified file path
    U32 readSequenceNumber(const char* filepath  //!< File path where sequence number is stored
    );

    //! Writes the sequence number to the specified file path
    U32 writeSequenceNumber(const char* filepath,  //!< File path where sequence number is stored
                            U32 value              //!< The sequence number to write
    );

    //! Reject packet that fails authentication
    void rejectPacket(Fw::Buffer& data, const ComCfg::FrameContext& contextOut);

    std::atomic<U32> m_sequenceNumber;             //!< The current sequence number
    std::atomic<U32> m_sequenceNumberWindow;       //!< The allowed window for sequence number validation
    std::atomic<U32> m_rejectedPacketsCount;       //!< Count of rejected packets for telemetry
    std::atomic<U32> m_authenticatedPacketsCount;  //!< Count of authenticated packets for telemetry
};

}  // namespace Components
