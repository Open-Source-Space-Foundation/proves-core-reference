#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "PROVESFlightControllerReference/Components/ComDelay/TickDivider.hpp"

using Components::TickDivider;

namespace {

// Tick indices (0-based) on which the divider releases, over the first `ticks` ticks.
std::vector<uint32_t> releaseTicks(uint16_t divider, uint32_t ticks) {
    TickDivider divider_state;
    std::vector<uint32_t> releases;
    for (uint32_t i = 0; i < ticks; i++) {
        if (divider_state.tick(divider)) {
            releases.push_back(i);
        }
    }
    return releases;
}

}  // namespace

class TickDividerPeriod : public ::testing::TestWithParam<uint16_t> {};

TEST_P(TickDividerPeriod, ReleasesEveryDividerPlusOneTicks) {
    const uint16_t divider = GetParam();
    const uint32_t period = static_cast<uint32_t>(divider) + 1;
    const std::vector<uint32_t> releases = releaseTicks(divider, 3 * period + 1);

    ASSERT_EQ(releases, (std::vector<uint32_t>{0, period, 2 * period, 3 * period}));
}

INSTANTIATE_TEST_SUITE_P(Dividers, TickDividerPeriod, ::testing::Values(0, 1, 29, 254, 255, 256, 299, 65535));

TEST(TickDividerTest, DefaultDividerAt10HzReleasesEvery30Seconds) {
    const std::vector<uint32_t> releases = releaseTicks(299, 301);
    ASSERT_EQ(releases, (std::vector<uint32_t>{0, 300}));
}

TEST(TickDividerTest, LoweringDividerBelowCountReleasesOnNextTick) {
    TickDivider divider_state;
    EXPECT_TRUE(divider_state.tick(299));
    for (int i = 0; i < 100; i++) {
        EXPECT_FALSE(divider_state.tick(299));
    }
    EXPECT_FALSE(divider_state.tick(29));
    EXPECT_TRUE(divider_state.tick(29));
}
