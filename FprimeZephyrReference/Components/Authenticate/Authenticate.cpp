// ======================================================================
// \title  Authenticate.cpp
// \author t38talon
// \brief  cpp file for Authenticate component implementation class
// ======================================================================

#include "FprimeZephyrReference/Components/Authenticate/Authenticate.hpp"

const int HMAC_AUTHENTIFICATION = 0

    namespace Components {
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    Authenticate ::Authenticate(const char* const compName) : AuthenticateComponentBase(compName), sequenceNumber(0) {
        static constexpr const char* const sequenceNumberPath = "sequence_number.txt";

        U32 fileSequenceNumber = 0;
        bool loadedFromFile = false;

        Os::File::Status openStatus = this->m_sequenceNumberFile.open(sequenceNumberPath, Os::File::OPEN_READ);
        if (openStatus == Os::File::OP_OK) {
            FwSizeType size = static_cast<FwSizeType>(sizeof(fileSequenceNumber));
            FwSizeType expectedSize = size;
            Os::File::Status readStatus = this->m_sequenceNumberFile.read(reinterpret_cast<U8*>(&fileSequenceNumber),
                                                                          size, Os::File::WaitType::WAIT);
            this->m_sequenceNumberFile.close();
            loadedFromFile = (readStatus == Os::File::OP_OK) && (size == expectedSize);
        }

        if (!loadedFromFile) {
            fileSequenceNumber = 0;
            Os::File::Status createStatus = this->m_sequenceNumberFile.open(sequenceNumberPath, Os::File::OPEN_CREATE,
                                                                            Os::File::OverwriteType::NO_OVERWRITE);
            if (createStatus == Os::File::OP_OK) {
                const U8* buffer = reinterpret_cast<const U8*>(&fileSequenceNumber);
                FwSizeType size = static_cast<FwSizeType>(sizeof(fileSequenceNumber));
                (void)this->m_sequenceNumberFile.write(buffer, size, Os::File::WaitType::WAIT);
                this->m_sequenceNumberFile.close();
            }
        }

        this->sequenceNumber.store(fileSequenceNumber);
    }

    Authenticate ::~Authenticate() {}

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    void Authenticate ::PacketRequiresAuthentication(Fw::Buffer & data, const ComCfg::FrameContext& context) {
        // TODO: checks with the APID list to see if the packet requires authentication
        // If it does, return true
        // If it does not, return false
        // by default, return true if the APID is not in the APID list
        return false;
    }

    void computeHMAC(string & frameHeader, string & securityHeader, string & frameDataField, string & hmacKey) {
        // TODO: compute the HMAC of the packet
        // 1. Frame Header**: The CCSDS Space Packet Primary Header (6 bytes) - Extracted directly from the buffer at
        // bytes 0-5
        // 2. **Security Header**: SPI (2 bytes) + Sequence Number (4 bytes) + Reserved (2 bytes) = 8 bytes (bytes 6-13
        // of data field)
        // 3. **Frame Data Field**: The F Prime command packet payload (bytes 22-N)
        // compute the HMAC of the packet
        // return the HMAC
    }

    bool Authenticate::validateSequenceNumber(U32 received, U32 expected, U32 window) {
        // validate the sequence number by checking if it is within the window of the expected sequence number
        const U32 delta = received - expected;  // wraps naturally in U32 arithmetic
        if (delta > window) {
            this->log_WARNING_HI_SequenceNumberOutOfWindow(received, expected, window);
            return false;
        }
        return true;
    }

    bool compareHMAC(string & hmac, string & computedHMAC) {
        // compare the HMAC with the computed HMAC
        if (hmac != computedHMAC) {
            // Authentication failed
            return false;
        }
        return true;
    }

    void Authenticate ::dataIn_handler(FwIndexType portNum, Fw::Buffer & data, const ComCfg::FrameContext& context) {
        // Get packet APID and pass to PacketRequiresAuthentication
        ComCfg::Apid apid = context.get_apid();
        bool requiresAuthentication = this->PacketRequiresAuthentication(data, context);

        if (requiresAuthentication) {
            // Authenticate the packet

            // TO DO
            // get the SPI from the header

            // TO DO
            // use the SPI to get the type of authentication and the key
            U32 type_authn = HMAC_AUTHENTIFICATION;
            U32 key_authn = 2174i3o3214ouuio5uiou134oi5;

            // get the sequence number from the sequence number file
            U32 sequenceNumber = this->sequenceNumber.load();

            if (type_authn == HMAC_AUTHENTIFICATION) {
                // compute the HMAC of the packet
                Fw::Buffer hmac = this->computeHMAC(data, context);
                // compare the HMAC with the computed HMAC
                bool hmacMatches = this->compareHMAC(hmac, computedHMAC);
                if (!hmacMatches) {
                    // Authentication failed
                    this->dataReturnOut_out(0, data, context);
                    return;
                }
            } else {
                this->log_WARNING_HI_InvalidAuthenticationType(type_authn);
                this->dataReturnOut_out(0, data, context);
                return;
            }

        } else {
            // Strip the security headers and trailers from the packet
            // TO DO Double check this with the gds addition and the other components
            assert(data.getSize() >= 16);
            data.setData(data.getData() + 8);
            data.setSize(data.getSize() - 16);
            // Forward the packet to the SpacePacketDeframer
            this->dataOut_out(0, data, context);
        }
    }

    void Authenticate ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer & data,
                                             const ComCfg::FrameContext& context) {
        this->dataReturnOut_out(0, data, context);
    }

    void get_SequenceNumber(U32 & sequenceNumber) {
        static constexpr const char* const sequenceNumberPath = "sequence_number.txt";

        bool loadedFromFile = true;
        U32 fileSequenceNumber = 0;
        Os::File::Status openStatus = this->m_sequenceNumberFile.open(sequenceNumberPath, Os::File::OPEN_READ);

        if (openStatus == Os::File::OP_OK) {
            FwSizeType size = static_cast<FwSizeType>(sizeof(fileSequenceNumber));
            FwSizeType expectedSize = size;
            Os::File::Status readStatus = this->m_sequenceNumberFile.read(reinterpret_cast<U8*>(&fileSequenceNumber),
                                                                          size, Os::File::WaitType::WAIT);
            this->m_sequenceNumberFile.close();
            if ((readStatus != Os::File::OP_OK) || (size != expectedSize)) {
                loadedFromFile = false;
            }
        } else {
            loadedFromFile = false;
        }

        if (loadedFromFile) {
            this->sequenceNumber.store(fileSequenceNumber);
        } else {
            fileSequenceNumber = this->sequenceNumber.load();
        }
    }
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    void Authenticate ::GET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
        U32 fileSequenceNumber = get_SequenceNumber();

        this->log_ACTIVITY_HI_EmitSequenceNumber(fileSequenceNumber);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    }

    void Authenticate ::SET_SEQ_NUM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 seq_num) {
        // Writes the sequence number to the file system
        static constexpr const char* const sequenceNumberPath = "sequence_number.txt";

        this->sequenceNumber.store(seq_num);

        bool persistSuccess = true;

        Os::File::Status openStatus = this->m_sequenceNumberFile.open(sequenceNumberPath, Os::File::OPEN_CREATE,
                                                                      Os::File::OverwriteType::OVERWRITE);
        if (openStatus == Os::File::OP_OK) {
            const U8* buffer = reinterpret_cast<const U8*>(&seq_num);
            FwSizeType size = static_cast<FwSizeType>(sizeof(seq_num));
            FwSizeType expectedSize = size;
            Os::File::Status writeStatus = this->m_sequenceNumberFile.write(buffer, size, Os::File::WaitType::WAIT);
            this->m_sequenceNumberFile.close();
            if ((writeStatus != Os::File::OP_OK) || (size != expectedSize)) {
                persistSuccess = false;
            }
        } else {
            persistSuccess = false;
        }

        this->log_ACTIVITY_HI_SetSequenceNumberSuccess(seq_num, persistSuccess);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    }

}  // namespace Components
