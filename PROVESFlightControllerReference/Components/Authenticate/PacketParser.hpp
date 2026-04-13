// ======================================================================
// \title  PacketParser.cpp
// \brief  hpp file for PacketParser implementation class
// ======================================================================

#pragma once

#include <array>

namespace Components {

class PacketParser final {
  private:
    // ----------------------------------------------------------------------
    //  Private constants
    // ----------------------------------------------------------------------

    static constexpr size_t kHeaderLength = 6;
    static constexpr size_t kHmacLength = 16;

  public:
    // ----------------------------------------------------------------------
    //  Public types
    // ----------------------------------------------------------------------

    //! PacketMeta stores information extracted from the packet
    struct PacketMeta {
        uint32_t opCode;                                     //!< Op code extracted from the packet body
        uint32_t sequenceNumber;                             //!< Sequence number extracted from the packet header
        std::array<std::uint8_t, kHmacLength> hmacTrailer;   //!< HMAC value extracted from the packet trailer
        std::array<std::uint8_t, kHmacLength> hmacComputed;  //!< HMAC value computed from the packet header and body
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
    bool parsePacket(const uint8_t* dataBuffer,  //!< The buffer containing the packet data
                     const size_t dataSize,      //!< The size of the packet data
                     PacketMeta& packetMeta      //!< The output packet metadata
    ) const;

  private:
    // ----------------------------------------------------------------------
    //  Private helper methods
    // ----------------------------------------------------------------------

    //! Parse the sequence number from the packet header
    bool parseSequenceNumber(const uint8_t* dataBuffer,  //!< The buffer containing the packet data
                             const size_t dataLength,    //!< The length of the packet data
                             uint32_t& sequenceNumber    //!< The sequence number
    ) const;

    //! Parse the opcode from the packet body
    bool parseOpCode(const uint8_t* dataBuffer,  //!< The buffer containing the packet data
                     const size_t dataLength,    //!< The length of the packet data
                     uint32_t& opCode            //!< The opcode
    ) const;

    //! Parse the HMAC from the packet trailer
    bool parseHmac(const uint8_t* dataBuffer,                   //!< The buffer containing the packet data
                   const size_t dataSize,                       //!< The size of the packet data
                   std::array<std::uint8_t, kHmacLength>& hmac  //!< The HMAC
    ) const;

    //! Compute the HMAC from the packet header and body
    bool computeHmac(const uint8_t* dataBuffer,                   //!< The buffer containing the packet data
                     const size_t dataSize,                       //!< The size of the packet data
                     std::array<std::uint8_t, kHmacLength>& hmac  //!< The computed HMAC
    ) const;

    //! Validate the sequence number against the expected value and allowed window
    bool validateSequenceNumber(uint32_t expected, uint32_t actual, uint32_t window) const;

    //! Validate the opcode is allowed to bypass authentication
    bool validateOpCodeBypassAllowed(uint32_t opCode) const;

    //! Validate the computed HMAC with the expected HMAC from the packet trailer
    bool validateHmac(const std::array<std::uint8_t, kHmacLength>& expected,
                      const std::array<std::uint8_t, kHmacLength>& actual) const;
};

}  // namespace Components
