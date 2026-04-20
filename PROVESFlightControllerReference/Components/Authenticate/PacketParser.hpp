// ======================================================================
// \title  PacketParser.cpp
// \brief  hpp file for PacketParser implementation class
// ======================================================================

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Components {

class PacketParser final {
  public:
    // ----------------------------------------------------------------------
    //  Public constants
    // ----------------------------------------------------------------------

    static constexpr size_t kHeaderLength = 6;
    static constexpr size_t kHmacLength = 16;

  public:
    // ----------------------------------------------------------------------
    //  Public types
    // ----------------------------------------------------------------------

    //! Result of parsing a packet
    enum class ParseResult {
        Ok,
        Bypass,
        SpiParseError,
        SpiValidationError,
        SequenceNumberParseError,
        SequenceNumberValidationError,
        OpCodeParseError,
        HmacParseError,
        HmacValidationError,
    };

  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct BDot object
    PacketParser();

    //! Destroy BDot object
    ~PacketParser();

  public:
    // ----------------------------------------------------------------------
    //  Public helper methods
    // ----------------------------------------------------------------------

    //! Parse the packet
    ParseResult parsePacket(const uint8_t* dataBuffer,              //!< The packet data buffer
                            const size_t dataSize,                  //!< The packet data size
                            const uint32_t expectedSequenceNumber,  //!< The expected sequence number
                            const uint32_t sequenceNumberWindow     //!< The allowed sequence number window
    ) const;

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Parse SPI from the packet header
    bool parseSpi(const uint8_t* dataBuffer,  //!< The packet data buffer
                  const size_t dataSize,      //!< The packet data size
                  uint32_t& spi               //!< The SPI
    ) const;

    //! Parse the sequence number from the packet header
    bool parseSequenceNumber(const uint8_t* dataBuffer,  //!< The packet data buffer
                             const size_t dataLength,    //!< The packet data size
                             uint32_t& sequenceNumber    //!< The sequence number
    ) const;

    //! Parse the opcode from the packet body
    bool parseOpCode(const uint8_t* dataBuffer,  //!< The packet data buffer
                     const size_t dataLength,    //!< The packet data size
                     uint32_t& opCode            //!< The opcode
    ) const;

    //! Parse the HMAC from the packet trailer
    bool parseHmac(const uint8_t* dataBuffer,              //!< The packet data buffer
                   const size_t dataSize,                  //!< The packet data size
                   std::array<uint8_t, kHmacLength>& hmac  //!< The HMAC
    ) const;

    //! Validate the SPI value from the packet header
    bool validateSpi(uint32_t spi  //!< The SPI
    ) const;

    //! Validate the sequence number against the expected value and allowed window
    bool validateSequenceNumber(uint32_t expected,  //!< The expected sequence number
                                uint32_t actual,    //!< The actual sequence number
                                uint32_t window     //!< The allowed sequence number window
    ) const;

    //! Validate the opcode is allowed to bypass authentication
    bool validateOpCodeBypassAllowed(uint32_t opCode  //!< The opcode
    ) const;

    //! Validate the computed HMAC with the expected HMAC from the packet trailer
    bool validateHmac(
        const uint8_t* dataBuffer,                            //!< The packet data buffer
        const size_t dataSize,                                //!< The packet data size
        const std::array<uint8_t, kHmacLength>& expectedHmac  //!< The expected HMAC extracted from the packet trailer
    ) const;
};

}  // namespace Components
