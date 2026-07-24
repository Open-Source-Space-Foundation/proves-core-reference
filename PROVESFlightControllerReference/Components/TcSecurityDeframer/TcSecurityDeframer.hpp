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

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/AuthKeyStoreArrayAc.hpp"
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

    //! Handler implementation for command PROVISION_KEY
    void PROVISION_KEY_cmdHandler(FwOpcodeType opCode,         //!< The opcode
                                  U32 cmdSeq,                  //!< The command sequence number
                                  U16 spi,                     //!< The SPI to provision
                                  const Fw::CmdStringArg& key  //!< The hex-encoded key to provision
                                  ) override;

    //! Handler implementation for command ADD_KEY
    void ADD_KEY_cmdHandler(FwOpcodeType opCode,         //!< The opcode
                            U32 cmdSeq,                  //!< The command sequence number
                            U16 spi,                     //!< The SPI to add
                            const Fw::CmdStringArg& key  //!< The hex-encoded key to add
                            ) override;

    //! Handler implementation for command REMOVE_KEY
    void REMOVE_KEY_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq,           //!< The command sequence number
                               U16 spi               //!< The SPI to remove
                               ) override;

  public:
    // ----------------------------------------------------------------------
    // Public helper methods
    // ----------------------------------------------------------------------

    //! Initialize component
    //!
    //! Loads the sequence number and key store from persistent storage
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

    //! Loads the key store from the file system into m_keyStore and (re)imports every valid slot
    //! into PSA, destroying any previously-imported keys first. Must be called with m_keyStoreLock held.
    //! On a missing/unreadable file, m_keyStore is left with no valid slots (keyless state).
    Os::File::Status loadKeyStore();

    //! Writes m_keyStore to the file system. Must be called with m_keyStoreLock held.
    Os::File::Status writeKeyStore();

    //! (Re)imports every valid slot in m_keyStore into PSA, destroying any previously-imported
    //! keys first, and updates m_keyIds. Must be called with m_keyStoreLock held.
    void importKeyStore();

    //! Finds the PSA key id for the given SPI among currently-imported keys.
    //! Must be called with m_keyStoreLock held.
    bool findKeyIdForSpi(uint32_t spi, uint32_t& keyId) const;

    //! Returns the number of valid slots in m_keyStore. Must be called with m_keyStoreLock held.
    U8 activeKeyCount() const;

    //! Projects m_keyStore's valid/spi fields into the plain-C++ ActiveSpiSlots type consumed by
    //! the pure-C++ Validator. Must be called with m_keyStoreLock held.
    ActiveSpiSlots activeSpiSlots() const;

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

    // Key store state is coupled between in-memory runtime state (m_keyStore/m_keyIds) and on-disk
    // persistent storage; protected by the same mutex to keep PSA-imported keys consistent with the
    // on-disk store shared across all TcSecurityDeframer instances (UART/LoRa/Sband)
    Os::Mutex m_keyStoreLock;               //!< Mutex protecting key store state atomicity
    Fw::String m_keyStoreFilePath;          //!< File path where the key store is stored
    AuthKeyStore m_keyStore;                //!< The active key store, up to 2 slots
    uint32_t m_keyIds[AuthKeyStore::SIZE];  //!< PSA key ids parallel to m_keyStore, valid iff the slot is valid
};

}  // namespace Components

#endif  // Components_TcSecurityDeframer
