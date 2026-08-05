// ======================================================================
// \title  PatchApplier.hpp
// \brief  hpp file for applying a delta patch to a reference image
// ======================================================================

#pragma once

#include <cstddef>
#include <cstdint>

namespace Components {

//! Applies a PROVES delta patch, reconstructing a new image from a reference image.
//!
//! A full image takes roughly 24 minutes to uplink at the ground station's pacing, which does not
//! fit a pass. A delta against an image already on board is a small fraction of that. The patch
//! encodes the classic bsdiff instruction stream: each record copies a run from the reference while
//! adding a per byte difference, appends a run of literal bytes that exist only in the new image,
//! then seeks the reference position.
//!
//! Applied as a streaming state machine so that neither image is ever held in RAM. The reference is
//! read randomly (it is resident in flash) and the output is written sequentially.
//!
//! Kept free of F Prime and Zephyr dependencies so it can be exercised by host unit tests against
//! patches produced by tools/bin/make-patch.
class PatchApplier {
  public:
    // ----------------------------------------------------------------------
    //  Public types
    // ----------------------------------------------------------------------

    //! Magic identifying a PROVES patch container: "PRVSPTCH"
    static constexpr uint8_t MAGIC[8] = {'P', 'R', 'V', 'S', 'P', 'T', 'C', 'H'};

    //! Container format version understood by this implementation
    static constexpr uint8_t FORMAT_VERSION = 1;

    //! Payload codec applied to the three instruction streams.
    //!
    //! The streams are decompressed before apply() sees them, so the codec only tells the caller
    //! what to do with the payload. An uncompressed patch is roughly the size of the image itself
    //! and has no uplink value; it exists for testing the container end to end.
    enum class Compression : uint8_t {
        NONE = 0,  //!< Streams stored verbatim
        LZSS = 1,  //!< Streams compressed together, see Components::LzssDecoder
    };

    //! Why an apply failed
    enum class Error : uint8_t {
        NONE = 0,                     //!< No error
        BAD_MAGIC = 1,                //!< Container magic did not match
        UNSUPPORTED_VERSION = 2,      //!< Container version is newer than this implementation
        UNSUPPORTED_COMPRESSION = 3,  //!< Container uses a codec this build cannot decode
        TRUNCATED_PATCH = 4,          //!< The patch ended before the new image was complete
        CORRUPT_CONTROL = 5,          //!< A control record would read or write outside the images
        REFERENCE_READ_FAILED = 6,    //!< Reading the reference image failed
        PATCH_READ_FAILED = 7,        //!< Reading the patch failed
        OUTPUT_WRITE_FAILED = 8,      //!< Writing the new image failed
        SIZE_MISMATCH = 9,            //!< The reconstructed image was not the size the patch declared
        WRONG_REFERENCE = 10,         //!< The on-board reference is not the one the patch was built from
    };

    //! Header of a PROVES patch container, as decoded from the wire
    struct Header {
        uint32_t new_size;         //!< Size in bytes of the reconstructed image
        uint32_t reference_size;   //!< Size in bytes of the reference the patch was built against
        uint32_t reference_crc32;  //!< CRC32 of that reference, in the form Os::File reports
        uint32_t control_size;     //!< Bytes of control records
        uint32_t diff_size;        //!< Bytes of the difference stream
        uint32_t extra_size;       //!< Bytes of the literal stream
        Compression compression;   //!< Codec applied to the three streams
    };

    //! Bytes occupied by the container header on the wire
    static constexpr size_t HEADER_SIZE = 8 + 1 + 1 + 4 + 4 + 4 + 4 + 4 + 4;

    //! Bytes occupied by one control record on the wire: three signed 32 bit values
    static constexpr size_t CONTROL_RECORD_SIZE = 12;

    //! One decoded control record
    struct Control {
        int32_t copy;   //!< Bytes to copy from the reference, adding the difference stream
        int32_t extra;  //!< Bytes to append verbatim from the literal stream
        int32_t seek;   //!< Signed adjustment applied to the reference position afterwards
    };

    //! Sources and sink the apply operates over.
    //!
    //! Implemented against files and flash in flight, and against memory buffers in unit tests.
    class Io {
      public:
        virtual ~Io() {}
        //! Read exactly size bytes of the reference image at the given offset
        virtual bool readReference(uint32_t offset, uint8_t* buffer, size_t size) = 0;
        //! Read exactly size bytes from a patch stream, advancing that stream's cursor
        virtual bool readControl(uint8_t* buffer, size_t size) = 0;
        virtual bool readDiff(uint8_t* buffer, size_t size) = 0;
        virtual bool readExtra(uint8_t* buffer, size_t size) = 0;
        //! Append size bytes to the reconstructed image
        virtual bool writeOutput(const uint8_t* buffer, size_t size) = 0;
    };

  public:
    // ----------------------------------------------------------------------
    //  Public helper methods
    // ----------------------------------------------------------------------

    //! Decode a container header from its wire representation.
    //!
    //! \param buffer: at least HEADER_SIZE bytes of header
    //! \param size: bytes available in buffer
    //! \param header: filled in on success
    //! \return NONE on success, otherwise the reason the header was rejected
    static Error decodeHeader(const uint8_t* buffer, size_t size, Header& header);

    //! Decode one control record from its wire representation
    static bool decodeControl(const uint8_t* buffer, size_t size, Control& control);

    //! Reconstruct the new image.
    //!
    //! \param header: decoded container header
    //! \param reference_size: size of the reference image, used to bound reads
    //! \param io: sources and sink to operate over
    //! \param scratch: working buffer, also bounding how much is processed at a time
    //! \param scratch_size: bytes available in scratch, must be at least 1
    //! \return NONE on success, otherwise the reason the apply failed
    static Error apply(const Header& header, uint32_t reference_size, Io& io, uint8_t* scratch, size_t scratch_size);
};

}  // namespace Components
