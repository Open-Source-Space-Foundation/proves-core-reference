// ======================================================================
// \title  PacketProcessor.hpp
// \brief  hpp file for PacketProcessor component implementation class
// ======================================================================

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Types/String.hpp>
#include <Os/File.hpp>
#include <Os/Mutex.hpp>
#include <atomic>
#include <cassert>

#include "PROVESFlightControllerReference/Components/PacketProcessor/Authenticator.hpp"
#include "PROVESFlightControllerReference/Components/PacketProcessor/PacketProcessorComponentAc.hpp"
#include "PROVESFlightControllerReference/Components/PacketProcessor/Parser.hpp"
#include "PROVESFlightControllerReference/Components/PacketProcessor/Validator.hpp"

namespace Components {

class PacketProcessor final : public PacketProcessorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PacketProcessor object
    PacketProcessor(const char* const compName  //!< The component name
    );

    //! Destroy PacketProcessor object
    ~PacketProcessor();

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
    void configure();

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    // Loads the sequence number from the specified file path
    Os::File::Status readSequenceNumber(U32& value  //!< The variable to store the read sequence number
    );

    //! Writes the sequence number to the specified file path
    Os::File::Status writeSequenceNumber(const U32 value  //!< The sequence number to write
    );

    //! Accepts a packet and updates the sequence number
    void acceptPacket(Fw::Buffer& data,                    //!< The buffer containing the packet data
                      const ComCfg::FrameContext& context  //!< The frame context associated with the packet
    );

    //! Bypasses a packet and updates the bypass packets count
    void bypassPacket(Fw::Buffer& data,                    //!< The buffer containing the packet data
                      const ComCfg::FrameContext& context  //!< The frame context associated with the packet
    );

    //! Forwards a packet to the output port
    void forwardPacket(Fw::Buffer& data,                    //!< The buffer containing the packet data
                       const ComCfg::FrameContext& context  //!< The frame context associated with the packet
    );

    //! Rejects a packet and updates the rejected packets count
    void rejectPacket(Fw::Buffer& data,                    //!< The buffer containing the packet data
                      const ComCfg::FrameContext& context  //!< The frame context associated with the packet
    );

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    // Sequence number state is coupled between in-memory runtime state and on-disk persistent storage
    // they are protected by the same mutex to ensure atomicity of updates across both mediums
    Os::Mutex m_sequenceNumberLock;       //!< Mutex protecting sequence number state atomicity
    Fw::String m_sequenceNumberFilePath;  //!< File path where sequence number is stored
    U32 m_sequenceNumber;                 //!< The current sequence number
    U32 m_sequenceNumberWindow;           //!< The allowed window for sequence number validation

    // Counters for telemetry
    std::atomic<U32> m_bypassPacketsCount;    //!< Count of packets that bypass authentication
    std::atomic<U32> m_rejectedPacketsCount;  //!< Count of rejected packets
};

}  // namespace Components
