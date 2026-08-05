// ======================================================================
// \title  SegmentPlan.hpp
// \brief  hpp file for uplink segment naming and validation helpers
// ======================================================================

#pragma once

#include <cstddef>
#include <cstdint>

namespace Components {

//! Naming and validation for the numbered segments an image is uplinked in.
//!
//! A full flight image does not fit in a single ground pass, and F Prime's file uplink has no
//! cross-pass resume, so an interrupted transfer loses everything sent so far. Uplinking the image
//! as numbered segments bounds that loss to one segment.
//!
//! Kept free of F Prime and Zephyr dependencies so it can be exercised by host unit tests.
class SegmentPlan {
  public:
    //! Largest number of segments an image may be split into.
    //!
    //! At the practical minimum segment size this is far more than a 1 MB slot requires, while
    //! keeping the assembly loop bounded.
    static constexpr uint16_t MAX_SEGMENTS = 999;

    //! Number of digits in the segment suffix, giving names like "/update/img.000"
    static constexpr uint8_t SUFFIX_DIGITS = 3;

    //! Whether a segment count can be assembled
    static bool isValidSegmentCount(uint16_t segments);

    //! Build the file name of one segment.
    //!
    //! Writes "<prefix>.NNN" into buffer, always null terminated. Returns false without writing a
    //! usable name when the index is out of range or the buffer is too small, so a caller that
    //! ignores the result cannot read an unterminated or truncated name.
    //!
    //! \param prefix: segment file name prefix, null terminated
    //! \param index: zero based segment index
    //! \param buffer: destination for the formatted name
    //! \param buffer_size: capacity of buffer in bytes, including the null terminator
    //! \return true when the full name was written
    static bool formatSegmentName(const char* prefix, uint16_t index, char* buffer, size_t buffer_size);

    //! Bytes needed to hold a segment name for a prefix of the given length, including the
    //! null terminator
    static size_t requiredNameSize(size_t prefix_length);
};

}  // namespace Components
