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

    //! Handler implementation for prepareForReboot
    //!
    //! Persists the exact current sequence number ahead of a planned reboot so
    //! ground and spacecraft resume aligned (no write-ahead gap to burn through)
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

    // Loads the sequence-number high-water record from the specified file path, validating its
    // torn-write checksum. On success, `value` holds the persisted high-water mark (see
    // SEQ_NUM_PERSIST_STRIDE below). On DOESNT_EXIST (genuine first boot) or any other failure --
    // including a checksum mismatch -- falls back to 0, the same as a first boot, and emits
    // SequenceNumberRecordInvalid for the latter cases so the anomaly is visible. Command
    // capability is never blocked on this outcome (see dataIn_handler for why).
    Os::File::Status readSequenceNumber(U32& value  //!< The variable to store the read high-water mark
    );

    //! Persists the sequence-number high-water record (value + torn-write checksum) to the
    //! specified file path. See writeAheadPersistIfNeeded() for when this is actually called --
    //! it is NOT called on every accepted frame (that was the root cause of issue #461).
    Os::File::Status writeSequenceNumber(const U32 value  //!< The high-water value to persist
    );

    //! Write-ahead batched persistence (issue #461 fix): call after accepting a frame with
    //! `acceptedSeqNum`. Persists a new high-water record when the persisted high-water mark has
    //! been reached or passed AND we are not in a post-failure backoff window, writing
    //! `acceptedSeqNum + SEQ_NUM_PERSIST_STRIDE` instead of the bare accepted value. This bounds
    //! filesystem writes to at most once per SEQ_NUM_PERSIST_STRIDE accepted frames in the steady
    //! state (eliminating the per-command race with FileUplink/FileManager/FileDownlink/PrmDb's own
    //! filesystem access -- see #461) while preserving the CORE INVARIANT: whenever a persist has
    //! ever succeeded, the value on disk is always >= the highest sequence number any legitimate
    //! command could have used before an unexpected power loss, so a replayed (already-used)
    //! sequence number is still rejected after a reboot.
    //!
    //! On a persist FAILURE, m_persistedHighWater is deliberately left UNCHANGED (unlike an earlier,
    //! incorrect version of this method that advanced it unconditionally -- that broke the
    //! invariant above: the on-disk value would stay stale/low while accepted sequence numbers kept
    //! advancing past it, reopening a real replay window for that gap after a reboot). Instead,
    //! failures schedule a bounded retry after SEQ_NUM_PERSIST_RETRY_BACKOFF further accepted
    //! frames -- not on every single subsequent frame (which is what caused the original #461 bug)
    //! and not silently abandoned either (SequenceNumberPersistFailed fires, throttled, with the
    //! raw fs status, every time a retry is attempted so a sustained failure is visible).
    void writeAheadPersistIfNeeded(U32 acceptedSeqNum  //!< The sequence number just accepted
    );

  private:
    // ----------------------------------------------------------------------
    // Private member variables
    // ----------------------------------------------------------------------

    //! Number of accepted frames between persisted high-water writes in the steady state (no
    //! failures). The persisted value is always `lastAccepted + SEQ_NUM_PERSIST_STRIDE` at the
    //! time of a successful write, so a reboot can lose at most this many already-used sequence
    //! numbers worth of "slack" before the anti-replay window catches up -- it can never lose the
    //! ability to reject a truly replayed frame, since the stored value never falls below any
    //! previously-accepted sequence number AT THE TIME OF A SUCCESSFUL WRITE.
    static constexpr U32 SEQ_NUM_PERSIST_STRIDE = 100;

    //! Number of accepted frames to wait before retrying a FAILED persist, rather than retrying on
    //! every subsequent accepted frame (which degrades to a persist-per-frame race, the original
    //! #461 bug) or leaving the on-disk value stale indefinitely (a latent replay-window risk).
    static constexpr U32 SEQ_NUM_PERSIST_RETRY_BACKOFF = 10;

    // Sequence number state is coupled between in-memory runtime state and on-disk persistent storage
    // they are protected by the same mutex to ensure atomicity of updates across both mediums
    Os::Mutex m_sequenceNumberLock;       //!< Mutex protecting sequence number state atomicity
    Fw::String m_sequenceNumberFilePath;  //!< File path where sequence number is stored
    U32 m_sequenceNumber;                 //!< The current (last accepted) sequence number
    U32 m_sequenceNumberWindow;           //!< The allowed window for sequence number validation
    U32 m_persistedHighWater;             //!< The high-water value last successfully written to disk
    //! Accepted-frame countdown before the next persist retry is attempted after a failure. 0 means
    //! "no backoff in effect" -- a normal persist attempt is due as soon as the stride condition is
    //! met. Set to SEQ_NUM_PERSIST_RETRY_BACKOFF after each failed attempt.
    U32 m_persistRetryBackoff;

    uint32_t m_hmacKeyId;  //!< The HMAC key ID used for authentication
};

}  // namespace Components

#endif  // Components_TcSecurityDeframer
