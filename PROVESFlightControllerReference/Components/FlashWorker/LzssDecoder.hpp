// ======================================================================
// \title  LzssDecoder.hpp
// \brief  hpp file for decoding the LZ77 stream a delta patch is carried in
// ======================================================================

#pragma once

#include <cstddef>
#include <cstdint>

namespace Components {

//! Decodes the LZ77 stream that tools/bin/make-patch.py wraps a delta patch in.
//!
//! A raw bsdiff patch is about the size of the image it rebuilds, so it is worth nothing over the
//! radio. Its difference stream is roughly 84% zero bytes in short, close-together runs, which this
//! coder collapses about 7x: a measured 728,388 byte patch becomes 101,171 bytes, turning ~24
//! minutes of uplink into ~3.3.
//!
//! The format is deliberately small and self contained rather than a third party library, so that
//! the exact decoder that flies can be round-tripped against real patches in host unit tests:
//!
//!   - Tokens are grouped in eights, preceded by one tag byte. Bit b of the tag is set when token
//!     b is a literal.
//!   - A literal is one byte, emitted as is.
//!   - A match is a little endian uint16 distance backwards, then one byte holding length minus 3,
//!     so lengths run 3 to 258. A match may overlap the bytes it is producing, which is how runs
//!     are encoded.
//!
//! Decoding streams in both directions and keeps only a WINDOW_SIZE ring buffer, so neither the
//! compressed patch nor the decoded result is ever held in RAM.
class LzssDecoder {
  public:
    // ----------------------------------------------------------------------
    //  Public types
    // ----------------------------------------------------------------------

    //! Bytes of history a match may reach back into. Measured against real patches, growing this
    //! to 64 KB saves under 8% of the compressed size, which does not pay for the RAM.
    static constexpr size_t WINDOW_SIZE = 4096;

    //! Shortest run worth encoding as a match rather than as literals
    static constexpr uint8_t MIN_MATCH = 3;

    //! Why a decode failed
    enum class Error : uint8_t {
        NONE = 0,                //!< No error
        TRUNCATED_INPUT = 1,     //!< The compressed stream ended before the output was complete
        BAD_DISTANCE = 2,        //!< A match reached back further than has been produced
        OUTPUT_OVERRUN = 3,      //!< A token would produce more output than was expected
        OUTPUT_WRITE_FAILED = 4  //!< Writing the decoded output failed
    };

    //! Compressed input, supplied a piece at a time
    class Source {
      public:
        virtual ~Source() {}
        //! Read exactly size bytes, returning false if that many are not available
        virtual bool read(uint8_t* buffer, size_t size) = 0;
    };

    //! Decoded output, consumed a piece at a time
    class Sink {
      public:
        virtual ~Sink() {}
        //! Append exactly size bytes
        virtual bool write(const uint8_t* buffer, size_t size) = 0;
    };

  public:
    // ----------------------------------------------------------------------
    //  Public helper methods
    // ----------------------------------------------------------------------

    //! Decode a stream.
    //!
    //! \param source: compressed input
    //! \param sink: destination for the decoded bytes
    //! \param expected_size: exact number of bytes the stream decodes to
    //! \param window: caller supplied history buffer of at least WINDOW_SIZE bytes
    //! \return NONE on success, otherwise the reason the decode failed
    static Error decode(Source& source, Sink& sink, uint32_t expected_size, uint8_t* window);
};

}  // namespace Components
