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

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Authenticator.hpp"
#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Parser.hpp"
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

    // Loads the sequence-number high-water record from the specified file path, validating its
    // torn-write checksum. On success, `value` holds the persisted high-water mark (see
    // SEQ_NUM_PERSIST_STRIDE below) and the component is armed. On DOESNT_EXIST (genuine first
    // boot), bootstraps to 0 and arms. On any other failure -- including a checksum mismatch --
    // the component is left UNARMED (see m_sequenceNumberArmed) rather than defaulting `value` to
    // a low number, since a wrong-but-plausible low value would reopen the anti-replay window.
    Os::File::Status readSequenceNumber(U32& value  //!< The variable to store the read high-water mark
    );

    //! Persists the sequence-number high-water record (value + torn-write checksum) to the
    //! specified file path. See writeAheadPersistIfNeeded() for when this is actually called --
    //! it is NOT called on every accepted frame (that was the root cause of issue #461).
    Os::File::Status writeSequenceNumber(const U32 value  //!< The high-water value to persist
    );

    //! Write-ahead batched persistence (issue #461 fix): call after accepting a frame with
    //! `acceptedSeqNum`. Persists a new high-water record ONLY when the persisted high-water mark
    //! has been reached or passed, writing `acceptedSeqNum + SEQ_NUM_PERSIST_STRIDE` instead of the
    //! bare accepted value. This bounds filesystem writes to at most once per
    //! SEQ_NUM_PERSIST_STRIDE accepted frames (eliminating the per-command race with
    //! FileUplink/FileManager/FileDownlink/PrmDb's own filesystem access -- see #461) while
    //! preserving the anti-replay invariant: the persisted value is always >= the highest sequence
    //! number any legitimate command could have used before an unexpected power loss, so a replayed
    //! (already-used) sequence number is still rejected after a reboot, by construction.
    void writeAheadPersistIfNeeded(U32 acceptedSeqNum  //!< The sequence number just accepted
    );

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    //! Number of accepted frames between persisted high-water writes. The persisted value is
    //! always `lastAccepted + SEQ_NUM_PERSIST_STRIDE` at the time of a write, so a reboot can lose
    //! at most this many already-used sequence numbers worth of "slack" before the anti-replay
    //! window catches up -- it can never lose the ability to reject a truly replayed frame, since
    //! the stored value never falls below any previously-accepted sequence number.
    static constexpr U32 SEQ_NUM_PERSIST_STRIDE = 100;

    // Sequence number state is coupled between in-memory runtime state and on-disk persistent storage
    // they are protected by the same mutex to ensure atomicity of updates across both mediums
    Os::Mutex m_sequenceNumberLock;       //!< Mutex protecting sequence number state atomicity
    Fw::String m_sequenceNumberFilePath;  //!< File path where sequence number is stored
    U32 m_sequenceNumber;                 //!< The current (last accepted) sequence number
    U32 m_sequenceNumberWindow;           //!< The allowed window for sequence number validation
    U32 m_persistedHighWater;             //!< The high-water value last written to persistent storage
    //! Whether the component will accept ANY frame. False after boot if the persisted record
    //! failed torn-write validation -- ground must issue SET_SEQ_NUM to re-arm (see
    //! SequenceNumberRecordInvalid). True after a genuine first boot (no file yet) or a
    //! successful record read/validation, and always true again immediately after SET_SEQ_NUM.
    bool m_sequenceNumberArmed;

    uint32_t m_hmacKeyId;  //!< The HMAC key ID used for authentication
};

}  // namespace Components

#endif  // Components_TcSecurityDeframer
