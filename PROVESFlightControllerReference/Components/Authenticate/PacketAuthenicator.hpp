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
        uint32_t opCode;                                    //!< Operation code extracted from the packet header
        uint32_t sequenceNumber;                            //!< Sequence number extracted from the packet header
        std::array<std::uint8_t, kHmacLength> hmacTrailer;  //!< HMAC value extracted from the packet trailer
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
}
