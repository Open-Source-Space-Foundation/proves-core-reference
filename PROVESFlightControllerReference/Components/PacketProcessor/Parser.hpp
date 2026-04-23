// ======================================================================
// \title  Parser.cpp
// \brief  hpp file for Parser implementation class
// ======================================================================

#pragma once

#include "Types.hpp"

namespace Components {
namespace PacketParser {

//! Status of parsing attempt
//! Must match the ParserStatus enum in the .fpp file
enum class Status {
    Ok,                        //!< Packet was successfully parsed
    SpiParseError,             //!< SPI could not be parsed from packet
    SequenceNumberParseError,  //!< Sequence number could not be parsed from packet
    OpCodeParseError,          //!< OpCode could not be parsed from packet
    HmacParseError,            //!< HMAC could not be parsed from packet
};

//! Result of parsing attempt
struct Result {
    Status status;  //!< The status of the parsing attempt
    Packet packet;  //!< The parsed packet
};

}  // namespace PacketParser

//! Parse packet fields from a raw buffer
PacketParser::Result parsePacket(const uint8_t* buffer,  //!< The packet data buffer
                                 const size_t size       //!< The packet data size
);

}  // namespace Components
