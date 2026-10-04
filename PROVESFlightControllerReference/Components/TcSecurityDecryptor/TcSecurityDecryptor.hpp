// ======================================================================
// \title  TcSecurityDecryptor.hpp
// \brief  hpp file for TcSecurityDecryptor component implementation class
// ======================================================================

#ifndef Components_TcSecurityDecryptor
#define Components_TcSecurityDecryptor

#include <FprimeExtras/Utilities/FileHelper/FileHelper.hpp>
#include <Fw/Types/String.hpp>
#include <Os/File.hpp>
#include <Os/Mutex.hpp>
#include <atomic>
#include <cassert>

#include "PROVESFlightControllerReference/Components/TcSecurityDecryptor/Authenticator.hpp"
#include "PROVESFlightControllerReference/Components/TcSecurityDecryptor/Parser.hpp"
#include "PROVESFlightControllerReference/Components/TcSecurityDecryptor/TcSecurityDecryptorComponentAc.hpp"
#include "PROVESFlightControllerReference/Components/TcSecurityDecryptor/Validator.hpp"

namespace Components {

class TcSecurityDecryptor final : public TcSecurityDecryptorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TcSecurityDecryptor object
    TcSecurityDecryptor(const char* const compName  //!< The component name
    );

    //! Destroy TcSecurityDecryptor object
    ~TcSecurityDecryptor();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for decryptIn
    //!
    //! Receives the security association index and [SeqNum | Data | Security Trailer] from the
    //! upstream Svc.Ccsds.CcsdsSdlsDeframer (which has already stripped the SA index out of the
    //! buffer), verifies the anti-replay sequence number and MAC, records the result in the frame
    //! context authenticated flag, and forwards the Data Field per CCSDS 355.0-B-2 §3.3.3.3.
    void decryptIn_handler(FwIndexType portNum,                 //!< The port number
                           U16 saIndex,                         //!< The security association index
                           Fw::Buffer& data,                    //!< The frame buffer
                           const ComCfg::FrameContext& context  //!< The frame context
                           ) override;

    //! Handler implementation for decryptReturnIn
    //!
    //! Returns ownership of buffers sent on decryptOut back to the upstream deframer
    void decryptReturnIn_handler(FwIndexType portNum,                 //!< The port number
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

    // Loads the sequence number from the specified file path
    Os::File::Status readSequenceNumber(U32& value  //!< The variable to store the read sequence number
    );

    //! Writes the sequence number to the specified file path
    Os::File::Status writeSequenceNumber(const U32 value  //!< The sequence number to write
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

    uint32_t m_hmacKeyId;  //!< The HMAC key ID used for authentication
};

}  // namespace Components

#endif  // Components_TcSecurityDecryptor
