// ======================================================================
// \title  PatchApplier.cpp
// \brief  cpp file for applying a delta patch to a reference image
// ======================================================================

#include "PatchApplier.hpp"

namespace Components {

// Out of line definitions so these can be odr-used (indexed, sized, bound to a reference) by
// callers. Required because these translation units are not all compiled as C++17, where static
// constexpr members would be implicitly inline.
constexpr uint8_t PatchApplier::MAGIC[8];
constexpr uint8_t PatchApplier::FORMAT_VERSION;
constexpr size_t PatchApplier::HEADER_SIZE;
constexpr size_t PatchApplier::CONTROL_RECORD_SIZE;

namespace {

//! Read a little endian unsigned 32 bit value
uint32_t readU32(const uint8_t* buffer) {
    return static_cast<uint32_t>(buffer[0]) | (static_cast<uint32_t>(buffer[1]) << 8) |
           (static_cast<uint32_t>(buffer[2]) << 16) | (static_cast<uint32_t>(buffer[3]) << 24);
}

//! Read a little endian signed 32 bit value without relying on signed overflow
int32_t readI32(const uint8_t* buffer) {
    const uint32_t raw = readU32(buffer);
    // Two's complement conversion that stays defined for the full range
    if (raw <= 0x7FFFFFFFU) {
        return static_cast<int32_t>(raw);
    }
    return static_cast<int32_t>(raw - 0x80000000U) - 0x7FFFFFFF - 1;
}

}  // namespace

PatchApplier::Error PatchApplier ::decodeHeader(const uint8_t* buffer, size_t size, Header& header) {
    if ((buffer == nullptr) || (size < PatchApplier::HEADER_SIZE)) {
        return Error::TRUNCATED_PATCH;
    }
    for (size_t i = 0; i < sizeof(PatchApplier::MAGIC); i++) {
        if (buffer[i] != PatchApplier::MAGIC[i]) {
            return Error::BAD_MAGIC;
        }
    }
    const uint8_t version = buffer[8];
    if (version != PatchApplier::FORMAT_VERSION) {
        return Error::UNSUPPORTED_VERSION;
    }
    const uint8_t compression = buffer[9];
    if ((compression != static_cast<uint8_t>(Compression::NONE)) &&
        (compression != static_cast<uint8_t>(Compression::LZSS))) {
        return Error::UNSUPPORTED_COMPRESSION;
    }
    header.compression = static_cast<Compression>(compression);
    header.new_size = readU32(&buffer[10]);
    header.reference_size = readU32(&buffer[14]);
    header.reference_crc32 = readU32(&buffer[18]);
    header.control_size = readU32(&buffer[22]);
    header.diff_size = readU32(&buffer[26]);
    header.extra_size = readU32(&buffer[30]);
    return Error::NONE;
}

bool PatchApplier ::decodeControl(const uint8_t* buffer, size_t size, Control& control) {
    if ((buffer == nullptr) || (size < PatchApplier::CONTROL_RECORD_SIZE)) {
        return false;
    }
    control.copy = readI32(&buffer[0]);
    control.extra = readI32(&buffer[4]);
    control.seek = readI32(&buffer[8]);
    return true;
}

PatchApplier::Error PatchApplier ::apply(const Header& header,
                                         uint32_t reference_size,
                                         Io& io,
                                         uint8_t* scratch,
                                         size_t scratch_size) {
    if ((scratch == nullptr) || (scratch_size < 2)) {
        return Error::CORRUPT_CONTROL;
    }
    // The streams reach us through Io already decompressed, so the codec is the caller's concern
    // Patching against the wrong reference silently produces a plausible but corrupt image, which
    // would then be flashed and booted. Refuse unless the reference is exactly the one the ground
    // built the patch from. The caller checks the content; the size is checked here.
    if (reference_size != header.reference_size) {
        return Error::WRONG_REFERENCE;
    }

    // The copy phase needs the reference bytes and the difference bytes at the same time
    const size_t half = scratch_size / 2;
    uint8_t* const reference_buffer = scratch;
    uint8_t* const diff_buffer = scratch + half;

    uint32_t new_position = 0;
    // Signed, and wider than the images, so that a hostile seek is caught rather than wrapping
    int64_t old_position = 0;

    while (new_position < header.new_size) {
        uint8_t record[PatchApplier::CONTROL_RECORD_SIZE];
        if (!io.readControl(record, sizeof(record))) {
            return Error::TRUNCATED_PATCH;
        }
        Control control;
        if (!PatchApplier::decodeControl(record, sizeof(record), control)) {
            return Error::TRUNCATED_PATCH;
        }
        if ((control.copy < 0) || (control.extra < 0)) {
            return Error::CORRUPT_CONTROL;
        }

        // Copy phase: reference bytes plus the per byte difference
        uint32_t remaining = static_cast<uint32_t>(control.copy);
        if (remaining > (header.new_size - new_position)) {
            return Error::CORRUPT_CONTROL;
        }
        if ((old_position < 0) ||
            ((old_position + static_cast<int64_t>(remaining)) > static_cast<int64_t>(reference_size))) {
            return Error::CORRUPT_CONTROL;
        }
        while (remaining > 0) {
            const size_t chunk = (remaining < half) ? static_cast<size_t>(remaining) : half;
            if (!io.readReference(static_cast<uint32_t>(old_position), reference_buffer, chunk)) {
                return Error::REFERENCE_READ_FAILED;
            }
            if (!io.readDiff(diff_buffer, chunk)) {
                return Error::PATCH_READ_FAILED;
            }
            for (size_t i = 0; i < chunk; i++) {
                // Wrapping addition is the bsdiff definition, not an overflow
                reference_buffer[i] = static_cast<uint8_t>(reference_buffer[i] + diff_buffer[i]);
            }
            if (!io.writeOutput(reference_buffer, chunk)) {
                return Error::OUTPUT_WRITE_FAILED;
            }
            old_position += static_cast<int64_t>(chunk);
            new_position += static_cast<uint32_t>(chunk);
            remaining -= static_cast<uint32_t>(chunk);
        }

        // Extra phase: bytes that exist only in the new image
        remaining = static_cast<uint32_t>(control.extra);
        if (remaining > (header.new_size - new_position)) {
            return Error::CORRUPT_CONTROL;
        }
        while (remaining > 0) {
            const size_t chunk = (remaining < scratch_size) ? static_cast<size_t>(remaining) : scratch_size;
            if (!io.readExtra(scratch, chunk)) {
                return Error::PATCH_READ_FAILED;
            }
            if (!io.writeOutput(scratch, chunk)) {
                return Error::OUTPUT_WRITE_FAILED;
            }
            new_position += static_cast<uint32_t>(chunk);
            remaining -= static_cast<uint32_t>(chunk);
        }

        old_position += control.seek;
    }

    if (new_position != header.new_size) {
        return Error::SIZE_MISMATCH;
    }
    return Error::NONE;
}

}  // namespace Components
