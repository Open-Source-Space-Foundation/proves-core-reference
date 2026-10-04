// ======================================================================
// \title  TcSecurityDeframer.hpp
// \brief  hpp file for TcSecurityDeframer component implementation class
// ======================================================================

#ifndef Components_TcSecurityDeframer
#define Components_TcSecurityDeframer

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Types/String.hpp>
#include <Os/File.hpp>
#include <Os/Mutex.hpp>
#include <atomic>
#include <cassert>

static_assert(ATOMIC_INT_LOCK_FREE == 2, "std::atomic<U32> must be lock-free on the target");

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Authenticator.hpp"
#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Parser.hpp"
#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Persistence.hpp"
#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/TcSecurityDeframerComponentAc.hpp"
#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Validator.hpp"

namespace Components {

class TcSecurityDeframer final : public TcSecurityDeframerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TcSecurityDeframer object
    TcSecurityDeframer(const char* const compName  //!< The component name
    );

    //! Destroy TcSecurityDeframer object
    ~TcSecurityDeframer();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for dataIn
    //!
    //! Receives [Security Header | Data Field | Security Trailer] from TcDeframer (which
    //! has already stripped the TC Primary Header and FECF), verifies the Security Header
    //! and MAC, records the result in the frame context authenticated flag, and forwards
    //! the Data Field per CCSDS 355.0-B-2 §3.3.3.3. Structurally invalid frames are
    //! returned upstream on dataReturnOut.
    void dataIn_handler(FwIndexType portNum,                 //!< The port number
                        Fw::Buffer& data,                    //!< The frame buffer
                        const ComCfg::FrameContext& context  //!< The frame context
                        ) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Returns ownership of buffers sent on dataOut back to the upstream component
    void dataReturnIn_handler(FwIndexType portNum,                 //!< The port number
                              Fw::Buffer& data,                    //!< The frame buffer
                              const ComCfg::FrameContext& context  //!< The frame context
                              ) override;

    //! Handler implementation for run
    //!
    //! Rate-group tick (1 Hz in the reference deployment). Persists the in-memory sequence number to
    //! SEQ_NUM_FILE_PATH when it differs from the last value known to be on disk. Runs on the
    //! rate-group thread under m_persistLock only, never m_sequenceNumberLock, so the filesystem
    //! write cannot delay dataIn.
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    //! Handler implementation for prepareForReboot
    //!
    //! Flushes the in-memory sequence number to SEQ_NUM_FILE_PATH before an intentional reboot, under
    //! m_persistLock so it cannot interleave with a run tick or SET_SEQ_NUM write.
    void prepareForReboot_handler(FwIndexType portNum  //!< The port number
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

    //! Persists the in-memory sequence number if it differs from the value known to be on disk.
    //! Takes m_persistLock only; never m_sequenceNumberLock.
    void persistIfChanged();

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    // m_sequenceNumberLock serializes validate-and-advance in dataIn against SET_SEQ_NUM, GET_SEQ_NUM and
    // configure(). The counter itself is atomic so run_handler can read it without that lock, keeping
    // SD-card I/O off the frame-processing thread.
    Os::Mutex m_sequenceNumberLock;       //!< Mutex serializing sequence number validate-and-advance
    Fw::String m_sequenceNumberFilePath;  //!< File path where sequence number is stored
    std::atomic<U32> m_sequenceNumber;    //!< The current (last accepted) sequence number
    U32 m_sequenceNumberWindow;           //!< The allowed window for sequence number validation

    // m_persistLock serializes the three filesystem writers (run_handler, prepareForReboot_handler and
    // SET_SEQ_NUM) and guards m_onDisk. dataIn never takes it. Lock order where both are
    // held: m_sequenceNumberLock then m_persistLock (SET_SEQ_NUM); the two signal handlers take
    // m_persistLock alone.
    Os::Mutex m_persistLock;               //!< Mutex serializing sequence number file I/O
    SequencePersistence::OnDisk m_onDisk;  //!< What the sequence number file is believed to hold

    uint32_t m_hmacKeyId;  //!< The HMAC key ID used for authentication
};

}  // namespace Components

#endif  // Components_TcSecurityDeframer
