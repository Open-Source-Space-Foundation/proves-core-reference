// ======================================================================
// \title  Parser.cpp
// \brief  hpp file for Parser implementation class
// ======================================================================

#pragma once

#include <cstdint>

#include "Types.hpp"

namespace Components {
namespace Ccsds355_0_B_2 {
namespace TcTransferFrame {
namespace Parser {

//! Status of parsing attempt
//! Must match the ParserStatus enum in the .fpp file
enum class Status {
    Ok,                        //!< Transfer frame was successfully parsed
    SpiParseError,             //!< SPI could not be parsed from transfer frame
    SequenceNumberParseError,  //!< Sequence number could not be parsed from transfer frame
    MacParseError,             //!< MAC could not be parsed from transfer frame
};

//! Result of parsing attempt
struct Result {
    Status status;                                      //!< The status of the parsing attempt
    Ccsds355_0_B_2::TCSecurityHeader securityHeader;    //!< The parsed security header
    Ccsds355_0_B_2::TCSecurityTrailer securityTrailer;  //!< The parsed security trailer
    Ccsds355_0_B_2::TCFrameData
        frameData;  //!< The unparsed frame data                                //!< The parsed OpCode
};

}  // namespace Parser
}  // namespace TcTransferFrame

//! Parse the TC Transfer Frame from a buffer and extract relevant information for validation and authentication
TcTransferFrame::Parser::Result parse(const uint8_t* buffer,  //!< The transfer frame buffer
                                      const size_t size       //!< The transfer frame size
);

}  // namespace Ccsds355_0_B_2
}  // namespace Components
