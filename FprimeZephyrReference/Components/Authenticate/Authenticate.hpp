// ======================================================================
// \title  Authenticate.hpp
// \author Ines
// \brief  hpp file for Authenticate component implementation class
// ======================================================================

#include <Os/File.hpp>
#include <atomic>
#include <cassert>
#include <string>

#include "FprimeZephyrReference/Components/Authenticate/AuthenticateComponentAc.hpp"

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
    void dataIn_handler(FwIndexType portNum,  //!< The port number
                        Fw::Buffer& data,
                        const ComCfg::FrameContext& context) override;

    //! Handler implementation for dataReturnIn
    //!
    //! Port receiving back ownership of buffers sent to dataOut
    void dataReturnIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& data,
                              const ComCfg::FrameContext& context) override;

  private:
    struct AuthenticationConfig {
        std::string type;
        std::string key;
    };

    AuthenticationConfig lookupAuthenticationConfig(U32 spi);

    bool PacketRequiresAuthentication(Fw::Buffer& data, const ComCfg::FrameContext& context);

    Fw::Buffer computeHMAC(const U8* securityHeader,
                           const FwSizeType securityHeaderLength,
                           const U8* commandPayload,
                           const FwSizeType commandPayloadLength,
                           const std::string& key);

    bool validateSequenceNumber(U32 received, U32 expected);

    bool compareHMAC(const U8* expected, const U8* actual, FwSizeType length) const;

    bool validateHMAC(const U8* securityHeader,
                      FwSizeType securityHeaderLength,
                      const U8* data,
                      FwSizeType dataLength,
                      const std::string& key,
                      const U8* securityTrailer);

    //! Validate and extract security header information
    //! \param data: Input buffer containing security header + data + security trailer
    //! \param contextOut: Frame context (modified if packet is rejected)
    //! \param securityHeader: Output parameter for extracted security header (6 bytes)
    //! \param securityTrailer: Output parameter for extracted security trailer (16 bytes)
    //! \param spi: Output parameter for Security Parameter Index
    //! \param sequenceNumber: Output parameter for sequence number
    //! \return true if header is valid and extracted, false if packet should be rejected
    bool validateHeader(Fw::Buffer& data,
                        ComCfg::FrameContext& contextOut,
                        U8* securityHeader,
                        U8* securityTrailer,
                        U32& spi,
                        U32& sequenceNumber);

    // function to get the current sequence number
    U32 get_SequenceNumber();

    // function to make sure files exist and are initialized
    U32 initializeFiles(const char* filePath);

    // function to read a U32 value from a file
    void persistToFile(const char* filePath, U32 value);

    // function to reject packets that fail authentication
    void rejectPacket(Fw::Buffer& data, ComCfg::FrameContext& contextOut);

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
                                U32 seq_num) override;

    //! Handler implementation for command GET_KEY_FROM_SPI
    void GET_KEY_FROM_SPI_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 spi) override;

    std::atomic<U32> sequenceNumber;
    Os::File m_sequenceNumberFile;
    Os::File m_rejectedPacketsCountFile;
    Os::File m_authenticatedPacketsCountFile;
    Os::File m_spiDictFile;
    std::atomic<U32> m_rejectedPacketsCount;
    std::atomic<U32> m_authenticatedPacketsCount;
};

}  // namespace Components
