// ======================================================================
// \title  test_FlashWorker_PatchApplier.cpp
// \brief  Unit tests for delta patch application
// ======================================================================

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "PROVESFlightControllerReference/Components/FlashWorker/PatchApplier.hpp"

using Components::PatchApplier;

namespace {

//! Sources and sink backed by memory, standing in for flash and the filesystem
class MemoryIo final : public PatchApplier::Io {
  public:
    MemoryIo(std::vector<uint8_t> reference,
             std::vector<uint8_t> control,
             std::vector<uint8_t> diff,
             std::vector<uint8_t> extra)
        : m_reference(std::move(reference)),
          m_control(std::move(control)),
          m_diff(std::move(diff)),
          m_extra(std::move(extra)) {}

    bool readReference(uint32_t offset, uint8_t* buffer, size_t size) override {
        if (m_fail_reference || (static_cast<size_t>(offset) + size > m_reference.size())) {
            return false;
        }
        std::memcpy(buffer, m_reference.data() + offset, size);
        return true;
    }
    bool readControl(uint8_t* buffer, size_t size) override { return take(m_control, m_control_pos, buffer, size); }
    bool readDiff(uint8_t* buffer, size_t size) override { return take(m_diff, m_diff_pos, buffer, size); }
    bool readExtra(uint8_t* buffer, size_t size) override { return take(m_extra, m_extra_pos, buffer, size); }
    bool writeOutput(const uint8_t* buffer, size_t size) override {
        if (m_fail_output) {
            return false;
        }
        m_output.insert(m_output.end(), buffer, buffer + size);
        return true;
    }

    const std::vector<uint8_t>& output() const { return m_output; }
    void failReference() { m_fail_reference = true; }
    void failOutput() { m_fail_output = true; }

  private:
    static bool take(const std::vector<uint8_t>& source, size_t& position, uint8_t* buffer, size_t size) {
        if (position + size > source.size()) {
            return false;
        }
        std::memcpy(buffer, source.data() + position, size);
        position += size;
        return true;
    }

    std::vector<uint8_t> m_reference, m_control, m_diff, m_extra, m_output;
    size_t m_control_pos = 0, m_diff_pos = 0, m_extra_pos = 0;
    bool m_fail_reference = false, m_fail_output = false;
};

//! Append a little endian int32
void putI32(std::vector<uint8_t>& out, int32_t value) {
    const uint32_t raw = static_cast<uint32_t>(value);
    out.push_back(static_cast<uint8_t>(raw & 0xFF));
    out.push_back(static_cast<uint8_t>((raw >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((raw >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((raw >> 24) & 0xFF));
}

//! Append one control record
void putControl(std::vector<uint8_t>& out, int32_t copy, int32_t extra, int32_t seek) {
    putI32(out, copy);
    putI32(out, extra);
    putI32(out, seek);
}

//! Build a decoded header. Reference CRC is unused by apply, which checks content via the caller.
PatchApplier::Header hdr(uint32_t new_size,
                         uint32_t reference_size,
                         uint32_t control_size,
                         uint32_t diff_size,
                         uint32_t extra_size) {
    PatchApplier::Header header{};
    header.new_size = new_size;
    header.reference_size = reference_size;
    header.reference_crc32 = 0;
    header.control_size = control_size;
    header.diff_size = diff_size;
    header.extra_size = extra_size;
    header.compression = PatchApplier::Compression::NONE;
    return header;
}

//! Build a valid header on the wire
std::vector<uint8_t> buildHeaderWire(uint32_t new_size,
                                     uint32_t reference_size,
                                     uint32_t reference_crc32,
                                     uint32_t control_size,
                                     uint32_t diff_size,
                                     uint32_t extra_size,
                                     uint8_t version = PatchApplier::FORMAT_VERSION,
                                     uint8_t codec = 0) {
    std::vector<uint8_t> out(PatchApplier::MAGIC, PatchApplier::MAGIC + sizeof(PatchApplier::MAGIC));
    out.push_back(version);
    out.push_back(codec);
    for (uint32_t value : {new_size, reference_size, reference_crc32, control_size, diff_size, extra_size}) {
        putI32(out, static_cast<int32_t>(value));
    }
    return out;
}

}  // namespace

// ----------------------------------------------------------------------
// Header decoding
// ----------------------------------------------------------------------

TEST(PatchApplierTest, DecodesAValidHeader) {
    const std::vector<uint8_t> wire = buildHeaderWire(1000, 900, 0xDEADBEEF, 24, 40, 60);
    ASSERT_EQ(PatchApplier::HEADER_SIZE, wire.size());
    PatchApplier::Header header{};
    EXPECT_EQ(PatchApplier::Error::NONE, PatchApplier::decodeHeader(wire.data(), wire.size(), header));
    EXPECT_EQ(1000u, header.new_size);
    EXPECT_EQ(900u, header.reference_size);
    EXPECT_EQ(0xDEADBEEFu, header.reference_crc32);
    EXPECT_EQ(24u, header.control_size);
    EXPECT_EQ(40u, header.diff_size);
    EXPECT_EQ(60u, header.extra_size);
    EXPECT_EQ(PatchApplier::Compression::NONE, header.compression);
}

TEST(PatchApplierTest, RejectsForeignOrCorruptHeaders) {
    const std::vector<uint8_t> wire = buildHeaderWire(10, 10, 0, 12, 0, 10);
    PatchApplier::Header header{};

    std::vector<uint8_t> bad_magic = wire;
    bad_magic[0] = 'X';
    EXPECT_EQ(PatchApplier::Error::BAD_MAGIC, PatchApplier::decodeHeader(bad_magic.data(), bad_magic.size(), header));

    // A newer container must be refused rather than misinterpreted
    const std::vector<uint8_t> future = buildHeaderWire(10, 10, 0, 12, 0, 10, PatchApplier::FORMAT_VERSION + 1);
    EXPECT_EQ(PatchApplier::Error::UNSUPPORTED_VERSION,
              PatchApplier::decodeHeader(future.data(), future.size(), header));

    // An unknown codec must be refused, not silently treated as verbatim
    const std::vector<uint8_t> compressed = buildHeaderWire(10, 10, 0, 12, 0, 10, PatchApplier::FORMAT_VERSION, 9);
    EXPECT_EQ(PatchApplier::Error::UNSUPPORTED_COMPRESSION,
              PatchApplier::decodeHeader(compressed.data(), compressed.size(), header));

    EXPECT_EQ(PatchApplier::Error::TRUNCATED_PATCH,
              PatchApplier::decodeHeader(wire.data(), PatchApplier::HEADER_SIZE - 1, header));
    EXPECT_EQ(PatchApplier::Error::TRUNCATED_PATCH, PatchApplier::decodeHeader(nullptr, 100, header));
}

// ----------------------------------------------------------------------
// Control decoding
// ----------------------------------------------------------------------

TEST(PatchApplierTest, DecodesSignedControlValues) {
    std::vector<uint8_t> wire;
    putControl(wire, 5, 3, -7);
    PatchApplier::Control control{};
    ASSERT_TRUE(PatchApplier::decodeControl(wire.data(), wire.size(), control));
    EXPECT_EQ(5, control.copy);
    EXPECT_EQ(3, control.extra);
    EXPECT_EQ(-7, control.seek);
}

TEST(PatchApplierTest, DecodesExtremeSignedControlValues) {
    std::vector<uint8_t> wire;
    putControl(wire, 0, 0, INT32_MIN);
    PatchApplier::Control control{};
    ASSERT_TRUE(PatchApplier::decodeControl(wire.data(), wire.size(), control));
    EXPECT_EQ(INT32_MIN, control.seek);
}

// ----------------------------------------------------------------------
// Reference binding
// ----------------------------------------------------------------------

TEST(PatchApplierTest, RefusesToPatchAgainstAReferenceOfTheWrongSize) {
    // Patching the wrong reference yields a plausible but corrupt image that would then be
    // flashed and booted, so a mismatch must stop the apply before it does any work
    const std::vector<uint8_t> reference = {1, 2, 3, 4};
    std::vector<uint8_t> control;
    putControl(control, 4, 0, 0);
    MemoryIo io(reference, control, std::vector<uint8_t>(4, 0), {});
    uint8_t scratch[8];
    EXPECT_EQ(PatchApplier::Error::WRONG_REFERENCE,
              PatchApplier::apply(hdr(4, 8, 12, 4, 0), 4, io, scratch, sizeof(scratch)));
    EXPECT_TRUE(io.output().empty());
}

// ----------------------------------------------------------------------
// Applying
// ----------------------------------------------------------------------

TEST(PatchApplierTest, ReconstructsAnIdenticalImage) {
    // One record copying the whole reference with a zero difference reproduces it exactly
    const std::vector<uint8_t> reference = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<uint8_t> control;
    putControl(control, 8, 0, 0);

    MemoryIo io(reference, control, std::vector<uint8_t>(8, 0), {});
    uint8_t scratch[16];
    EXPECT_EQ(PatchApplier::Error::NONE, PatchApplier::apply(hdr(8, 8, 12, 8, 0), 8, io, scratch, sizeof(scratch)));
    EXPECT_EQ(reference, io.output());
}

TEST(PatchApplierTest, AppliesPerByteDifferences) {
    const std::vector<uint8_t> reference = {10, 20, 30, 40};
    std::vector<uint8_t> control;
    putControl(control, 4, 0, 0);

    MemoryIo io(reference, control, std::vector<uint8_t>({1, 2, 3, 4}), {});
    uint8_t scratch[16];
    EXPECT_EQ(PatchApplier::Error::NONE, PatchApplier::apply(hdr(4, 4, 12, 4, 0), 4, io, scratch, sizeof(scratch)));
    EXPECT_EQ(std::vector<uint8_t>({11, 22, 33, 44}), io.output());
}

TEST(PatchApplierTest, DifferenceAdditionWrapsRatherThanSaturating) {
    // Wrapping addition is the bsdiff definition; saturating here would corrupt the image
    const std::vector<uint8_t> reference = {250, 0};
    std::vector<uint8_t> control;
    putControl(control, 2, 0, 0);

    MemoryIo io(reference, control, std::vector<uint8_t>({10, 255}), {});
    uint8_t scratch[8];
    EXPECT_EQ(PatchApplier::Error::NONE, PatchApplier::apply(hdr(2, 2, 12, 2, 0), 2, io, scratch, sizeof(scratch)));
    EXPECT_EQ(std::vector<uint8_t>({4, 255}), io.output());
}

TEST(PatchApplierTest, AppendsLiteralBytesFromTheExtraStream) {
    const std::vector<uint8_t> reference = {1, 2};
    std::vector<uint8_t> control;
    putControl(control, 2, 3, 0);

    MemoryIo io(reference, control, std::vector<uint8_t>(2, 0), std::vector<uint8_t>({77, 88, 99}));
    uint8_t scratch[8];
    EXPECT_EQ(PatchApplier::Error::NONE, PatchApplier::apply(hdr(5, 2, 12, 2, 3), 2, io, scratch, sizeof(scratch)));
    EXPECT_EQ(std::vector<uint8_t>({1, 2, 77, 88, 99}), io.output());
}

TEST(PatchApplierTest, SeekMovesTheReferencePositionBothWays) {
    // Reconstruct {3,4,1,2} from {1,2,3,4}: skip ahead, copy the tail, rewind, copy the head
    const std::vector<uint8_t> reference = {1, 2, 3, 4};
    std::vector<uint8_t> control;
    putControl(control, 0, 0, 2);
    putControl(control, 2, 0, -4);
    putControl(control, 2, 0, 0);

    MemoryIo io(reference, control, std::vector<uint8_t>(4, 0), {});
    uint8_t scratch[8];
    EXPECT_EQ(PatchApplier::Error::NONE, PatchApplier::apply(hdr(4, 4, 36, 4, 0), 4, io, scratch, sizeof(scratch)));
    EXPECT_EQ(std::vector<uint8_t>({3, 4, 1, 2}), io.output());
}

TEST(PatchApplierTest, WorksWithATinyScratchBuffer) {
    // The apply must chunk correctly when the buffer is far smaller than the runs
    const std::vector<uint8_t> reference = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<uint8_t> control;
    putControl(control, 10, 4, 0);

    MemoryIo io(reference, control, std::vector<uint8_t>(10, 1), std::vector<uint8_t>({100, 101, 102, 103}));
    uint8_t scratch[2];  // one byte per half
    EXPECT_EQ(PatchApplier::Error::NONE, PatchApplier::apply(hdr(14, 10, 12, 10, 4), 10, io, scratch, sizeof(scratch)));
    EXPECT_EQ(std::vector<uint8_t>({2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 100, 101, 102, 103}), io.output());
}

// ----------------------------------------------------------------------
// Malformed and hostile patches
//
// A patch arrives over the radio, so the applier must refuse to read or write outside the
// images rather than trusting the control stream.
// ----------------------------------------------------------------------

TEST(PatchApplierTest, RejectsNegativeCopyOrExtra) {
    const std::vector<uint8_t> reference = {1, 2, 3, 4};
    for (int field = 0; field < 2; field++) {
        std::vector<uint8_t> control;
        putControl(control, (field == 0) ? -1 : 1, (field == 0) ? 1 : -1, 0);
        MemoryIo io(reference, control, std::vector<uint8_t>(4, 0), {});
        uint8_t scratch[8];
        EXPECT_EQ(PatchApplier::Error::CORRUPT_CONTROL,
                  PatchApplier::apply(hdr(4, 4, 12, 4, 0), 4, io, scratch, sizeof(scratch)));
    }
}

TEST(PatchApplierTest, RejectsCopyRunningPastTheReference) {
    const std::vector<uint8_t> reference = {1, 2, 3, 4};
    std::vector<uint8_t> control;
    putControl(control, 8, 0, 0);  // more than the reference holds
    MemoryIo io(reference, control, std::vector<uint8_t>(8, 0), {});
    uint8_t scratch[8];
    EXPECT_EQ(PatchApplier::Error::CORRUPT_CONTROL,
              PatchApplier::apply(hdr(8, 4, 12, 8, 0), 4, io, scratch, sizeof(scratch)));
}

TEST(PatchApplierTest, RejectsWritingPastTheDeclaredImageSize) {
    const std::vector<uint8_t> reference = {1, 2, 3, 4};
    std::vector<uint8_t> control;
    putControl(control, 4, 0, 0);  // writes 4 bytes into a 2 byte image
    MemoryIo io(reference, control, std::vector<uint8_t>(4, 0), {});
    uint8_t scratch[8];
    EXPECT_EQ(PatchApplier::Error::CORRUPT_CONTROL,
              PatchApplier::apply(hdr(2, 4, 12, 4, 0), 4, io, scratch, sizeof(scratch)));
}

TEST(PatchApplierTest, RejectsSeekingBeforeTheStartOfTheReference) {
    const std::vector<uint8_t> reference = {1, 2, 3, 4};
    std::vector<uint8_t> control;
    putControl(control, 0, 0, -100);  // seek far negative
    putControl(control, 1, 0, 0);     // then try to read
    MemoryIo io(reference, control, std::vector<uint8_t>(4, 0), {});
    uint8_t scratch[8];
    EXPECT_EQ(PatchApplier::Error::CORRUPT_CONTROL,
              PatchApplier::apply(hdr(1, 4, 24, 4, 0), 4, io, scratch, sizeof(scratch)));
}

TEST(PatchApplierTest, ReportsATruncatedControlStream) {
    const std::vector<uint8_t> reference = {1, 2, 3, 4};
    MemoryIo io(reference, {}, {}, {});  // no control records at all
    uint8_t scratch[8];
    EXPECT_EQ(PatchApplier::Error::TRUNCATED_PATCH,
              PatchApplier::apply(hdr(4, 4, 0, 0, 0), 4, io, scratch, sizeof(scratch)));
}

TEST(PatchApplierTest, ReportsATruncatedDiffStream) {
    const std::vector<uint8_t> reference = {1, 2, 3, 4};
    std::vector<uint8_t> control;
    putControl(control, 4, 0, 0);
    MemoryIo io(reference, control, std::vector<uint8_t>(2, 0), {});  // diff is short
    uint8_t scratch[8];
    EXPECT_EQ(PatchApplier::Error::PATCH_READ_FAILED,
              PatchApplier::apply(hdr(4, 4, 12, 4, 0), 4, io, scratch, sizeof(scratch)));
}

TEST(PatchApplierTest, PropagatesIoFailures) {
    const std::vector<uint8_t> reference = {1, 2, 3, 4};
    std::vector<uint8_t> control;
    putControl(control, 4, 0, 0);
    uint8_t scratch[8];

    MemoryIo reference_failure(reference, control, std::vector<uint8_t>(4, 0), {});
    reference_failure.failReference();
    EXPECT_EQ(PatchApplier::Error::REFERENCE_READ_FAILED,
              PatchApplier::apply(hdr(4, 4, 12, 4, 0), 4, reference_failure, scratch, sizeof(scratch)));

    MemoryIo output_failure(reference, control, std::vector<uint8_t>(4, 0), {});
    output_failure.failOutput();
    EXPECT_EQ(PatchApplier::Error::OUTPUT_WRITE_FAILED,
              PatchApplier::apply(hdr(4, 4, 12, 4, 0), 4, output_failure, scratch, sizeof(scratch)));
}

TEST(PatchApplierTest, RejectsAnUnusableScratchBuffer) {
    const std::vector<uint8_t> reference = {1, 2};
    MemoryIo io(reference, {}, {}, {});
    uint8_t scratch[2];
    EXPECT_EQ(PatchApplier::Error::CORRUPT_CONTROL, PatchApplier::apply(hdr(2, 2, 0, 0, 0), 2, io, nullptr, 8));
    EXPECT_EQ(PatchApplier::Error::CORRUPT_CONTROL, PatchApplier::apply(hdr(2, 2, 0, 0, 0), 2, io, scratch, 1));
}

TEST(PatchApplierTest, AnEmptyImageNeedsNoRecords) {
    const std::vector<uint8_t> reference = {1, 2};
    MemoryIo io(reference, {}, {}, {});
    uint8_t scratch[8];
    EXPECT_EQ(PatchApplier::Error::NONE, PatchApplier::apply(hdr(0, 2, 0, 0, 0), 2, io, scratch, sizeof(scratch)));
    EXPECT_TRUE(io.output().empty());
}
