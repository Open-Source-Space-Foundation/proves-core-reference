#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/Drv/RtcManager/RtcHelper.hpp"

using Drv::RescaledTime;
using Drv::RtcHelper;

TEST(RtcHelperTest, BasicRescaleUseconds) {
    RtcHelper helper;

    // First sample establishes offset, returns 0
    EXPECT_EQ(helper.rescaleUseconds(0U, 0U).useconds, 0U);

    // Move forward within same second
    EXPECT_EQ(helper.rescaleUseconds(0U, 123456U).useconds, 123456U);

    // Move to next second, offset updates
    EXPECT_EQ(helper.rescaleUseconds(1U, 250U).useconds, 0U);

    // Small forward step
    EXPECT_EQ(helper.rescaleUseconds(1U, 500U).useconds, 250U);
}

TEST(RtcHelperTest, BasicRescaleUsecondsHasNoSecondsCarry) {
    RtcHelper helper;

    EXPECT_EQ(helper.rescaleUseconds(0U, 0U).seconds_carry, 0U);
    EXPECT_EQ(helper.rescaleUseconds(0U, 123456U).seconds_carry, 0U);
    EXPECT_EQ(helper.rescaleUseconds(1U, 250U).seconds_carry, 0U);
    EXPECT_EQ(helper.rescaleUseconds(1U, 500U).seconds_carry, 0U);
}

TEST(RtcHelperTest, WrapAroundAtOneSecond) {
    RtcHelper helper;

    // Prime offset near the end of the second
    EXPECT_EQ(helper.rescaleUseconds(0U, 999999U).useconds, 0U);

    // Wrap from 999999 -> 85: only 86 microseconds of real time passed (the offset was primed
    // right at the edge of the cycle), so no whole second has elapsed yet
    const RescaledTime result = helper.rescaleUseconds(0U, 85U);
    EXPECT_EQ(result.useconds, 86U);
    EXPECT_EQ(result.seconds_carry, 0U);
}

TEST(RtcHelperTest, WrapAroundAtU32Max) {
    RtcHelper helper;

    // Prime offset near the max U32 value
    EXPECT_EQ(helper.rescaleUseconds(0U, 4294967290U).useconds, 0U);

    // Wrap from 4294967290 -> 5: uint32 arithmetic wraps through the full 32-bit space (not just
    // the 1,000,000us domain), producing a large elapsed value that folds into a carry
    const RescaledTime result = helper.rescaleUseconds(0U, 5U);
    EXPECT_EQ(result.useconds, 11U);
    EXPECT_EQ(result.seconds_carry, 1U);
}

TEST(RtcHelperTest, StaleRtcSecondAcrossMultipleWraps) {
    RtcHelper helper;

    // RTC second is stale across a burst of reads while uptime keeps advancing, wrapping twice
    EXPECT_EQ(helper.rescaleUseconds(0U, 999999U).useconds, 0U);

    const RescaledTime one_wrap = helper.rescaleUseconds(0U, 999998U);
    EXPECT_EQ(one_wrap.useconds, 999999U);
    EXPECT_EQ(one_wrap.seconds_carry, 0U);

    // Uptime wraps a second time while current_seconds (0) still hasn't ticked over
    const RescaledTime two_wraps = helper.rescaleUseconds(0U, 999997U);
    EXPECT_EQ(two_wraps.useconds, 999998U);
    EXPECT_EQ(two_wraps.seconds_carry, 1U);
}
