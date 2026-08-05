// ======================================================================
// \title  test_ImageBuilder_SegmentPlan.cpp
// \brief  Unit tests for uplink segment naming and validation
// ======================================================================

#include <gtest/gtest.h>

#include <string>

#include "PROVESFlightControllerReference/Components/FlashWorker/SegmentPlan.hpp"

using Components::SegmentPlan;

namespace {

//! Format a segment name into a generously sized buffer
std::string name(const char* prefix, uint16_t index, size_t buffer_size = 64) {
    std::string buffer(buffer_size, '\xFF');
    const bool ok = SegmentPlan::formatSegmentName(prefix, index, &buffer[0], buffer_size);
    EXPECT_TRUE(ok) << "formatting segment " << index << " of " << prefix;
    return std::string(buffer.c_str());
}

}  // namespace

// ----------------------------------------------------------------------
// Segment counts
// ----------------------------------------------------------------------

TEST(SegmentPlanTest, RejectsEmptyAndOversizedCounts) {
    EXPECT_FALSE(SegmentPlan::isValidSegmentCount(0));
    EXPECT_TRUE(SegmentPlan::isValidSegmentCount(1));
    EXPECT_TRUE(SegmentPlan::isValidSegmentCount(SegmentPlan::MAX_SEGMENTS));
    EXPECT_FALSE(SegmentPlan::isValidSegmentCount(SegmentPlan::MAX_SEGMENTS + 1));
}

// ----------------------------------------------------------------------
// Naming
// ----------------------------------------------------------------------

TEST(SegmentPlanTest, NamesAreZeroPaddedToFixedWidth) {
    // Fixed width keeps segment names sorting in transfer order
    EXPECT_EQ("/update/img.000", name("/update/img", 0));
    EXPECT_EQ("/update/img.001", name("/update/img", 1));
    EXPECT_EQ("/update/img.042", name("/update/img", 42));
    EXPECT_EQ("/update/img.998", name("/update/img", 998));
}

TEST(SegmentPlanTest, NamesSortInTransferOrder) {
    EXPECT_LT(name("/u/i", 0), name("/u/i", 1));
    EXPECT_LT(name("/u/i", 9), name("/u/i", 10));
    EXPECT_LT(name("/u/i", 99), name("/u/i", 100));
}

TEST(SegmentPlanTest, RejectsIndexAtOrAboveTheLimit) {
    char buffer[64];
    EXPECT_FALSE(SegmentPlan::formatSegmentName("/u/i", SegmentPlan::MAX_SEGMENTS, buffer, sizeof(buffer)));
    // Even on rejection the buffer must hold a usable, terminated string
    EXPECT_STREQ("", buffer);
}

TEST(SegmentPlanTest, RejectsBuffersThatCannotHoldTheName) {
    const char* prefix = "/update/img";
    const size_t needed = SegmentPlan::requiredNameSize(11);
    EXPECT_EQ(16u, needed);  // 11 prefix + '.' + 3 digits + null

    char exact[16];
    EXPECT_TRUE(SegmentPlan::formatSegmentName(prefix, 7, exact, sizeof(exact)));
    EXPECT_STREQ("/update/img.007", exact);

    // One byte short must fail rather than truncate
    char small[15];
    EXPECT_FALSE(SegmentPlan::formatSegmentName(prefix, 7, small, sizeof(small)));
    EXPECT_STREQ("", small);
}

TEST(SegmentPlanTest, RejectsNullArguments) {
    char buffer[64];
    EXPECT_FALSE(SegmentPlan::formatSegmentName(nullptr, 0, buffer, sizeof(buffer)));
    EXPECT_FALSE(SegmentPlan::formatSegmentName("/u/i", 0, nullptr, sizeof(buffer)));
    EXPECT_FALSE(SegmentPlan::formatSegmentName("/u/i", 0, buffer, 0));
}

TEST(SegmentPlanTest, HandlesAnEmptyPrefix) {
    EXPECT_EQ(".005", name("", 5));
}
