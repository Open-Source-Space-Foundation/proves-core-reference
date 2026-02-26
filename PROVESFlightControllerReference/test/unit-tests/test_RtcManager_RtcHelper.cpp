#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/Drv/RtcManager/RtcHelper.hpp"

using Drv::RtcHelper;

TEST(RtcHelperTest, BasicRescaleUseconds) {
    RtcHelper helper;

    // First sample establishes offset, returns 0
    EXPECT_EQ(helper.rescaleUseconds(0U, 0U), 0U);

    // Move forward within same second
    EXPECT_EQ(helper.rescaleUseconds(0U, 123456U), 123456U);

    // Move to next second, offset updates
    EXPECT_EQ(helper.rescaleUseconds(1U, 250U), 0U);

    // Small forward step
    EXPECT_EQ(helper.rescaleUseconds(1U, 500U), 250U);
}

TEST(RtcHelperTest, WrapAroundAtOneSecond) {
    RtcHelper helper;

    // Prime offset near the end of the second
    EXPECT_EQ(helper.rescaleUseconds(0U, 999999U), 0U);

    // Wrap from 999999 -> 85: forward delta is 86 microseconds
    EXPECT_EQ(helper.rescaleUseconds(0U, 85U), 86U);
}

TEST(RtcHelperTest, WrapAroundAtU32Max) {
    RtcHelper helper;

    // Prime offset near the max U32 value
    EXPECT_EQ(helper.rescaleUseconds(0U, 4294967290U), 0U);

    // Wrap from 4294967290 -> 5: forward delta is 11 microseconds
    EXPECT_EQ(helper.rescaleUseconds(0U, 5U), 11U);
}
