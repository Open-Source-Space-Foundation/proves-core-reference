#include <gtest/gtest.h>

#include <vector>

#include "PROVESFlightControllerReference/Components/StackMonitor/StackMonitorCore.hpp"

using Components::StackMonitorCore;
using Components::ThreadStackSample;

// Warn when a thread's free stack drops below 20% of its own size.
const std::uint32_t WARN_THRESHOLD_PERCENT = 20;

TEST(StackMonitorCoreTest, SummarizesWorstThreadFromSampleSet) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    std::vector<ThreadStackSample> samples = {
        {"idle", 1024, 900},       // 12% used, healthy
        {"sband", 4096, 1024},     // 75% used, worst of the three
        {"watchdog", 2048, 1800},  // ~12% used, healthy
    };

    auto result = core.tick(samples);

    EXPECT_EQ(result.summary.worstThreadName, "sband");
    EXPECT_EQ(result.summary.worstThreadFreeBytes, 1024u);
    EXPECT_EQ(result.summary.worstThreadUsedPercent, 75u);
    EXPECT_EQ(result.summary.threadsBelowThreshold, 0u);
}

TEST(StackMonitorCoreTest, WarnsWhenThreadCrossesBelowThreshold) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    // sband: 4096 total, 700 free => ~17% free, below the 20% threshold.
    std::vector<ThreadStackSample> samples = {
        {"sband", 4096, 700},
    };

    auto result = core.tick(samples);

    EXPECT_EQ(result.summary.threadsBelowThreshold, 1u);
    ASSERT_EQ(result.newWarnings.size(), 1u);
    EXPECT_EQ(result.newWarnings[0].name, "sband");
    EXPECT_EQ(result.newWarnings[0].freeBytes, 700u);
    EXPECT_EQ(result.newWarnings[0].sizeBytes, 4096u);
    EXPECT_TRUE(result.newRecoveries.empty());
}

TEST(StackMonitorCoreTest, DoesNotReWarnWhileStillBelowThreshold) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    std::vector<ThreadStackSample> samples = {
        {"sband", 4096, 700},  // below threshold
    };

    auto first = core.tick(samples);
    ASSERT_EQ(first.newWarnings.size(), 1u);

    // Still below threshold on the next tick: no new warning (hysteresis).
    auto second = core.tick(samples);
    EXPECT_TRUE(second.newWarnings.empty());
    EXPECT_EQ(second.summary.threadsBelowThreshold, 1u);
}

TEST(StackMonitorCoreTest, ClearsWhenThreadRecoversAboveThreshold) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    std::vector<ThreadStackSample> low = {
        {"sband", 4096, 700},  // ~17% free, below threshold
    };
    auto warned = core.tick(low);
    ASSERT_EQ(warned.newWarnings.size(), 1u);

    std::vector<ThreadStackSample> recovered = {
        {"sband", 4096, 2048},  // 50% free, back above threshold
    };
    auto result = core.tick(recovered);

    EXPECT_TRUE(result.newWarnings.empty());
    ASSERT_EQ(result.newRecoveries.size(), 1u);
    EXPECT_EQ(result.newRecoveries[0].name, "sband");
    EXPECT_EQ(result.summary.threadsBelowThreshold, 0u);

    // Crossing below threshold again after recovery re-arms the warning.
    auto rewarned = core.tick(low);
    ASSERT_EQ(rewarned.newWarnings.size(), 1u);
    EXPECT_EQ(rewarned.newWarnings[0].name, "sband");
}

TEST(StackMonitorCoreTest, EmptySampleSetProducesNoCrashAndNeutralSummary) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    auto result = core.tick({});

    EXPECT_EQ(result.summary.worstThreadName, "");
    EXPECT_EQ(result.summary.worstThreadFreeBytes, 0u);
    EXPECT_EQ(result.summary.threadsBelowThreshold, 0u);
    EXPECT_TRUE(result.newWarnings.empty());
    EXPECT_TRUE(result.newRecoveries.empty());
}

TEST(StackMonitorCoreTest, ThreadDisappearingBetweenTicksDoesNotAssert) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    std::vector<ThreadStackSample> withSband = {
        {"sband", 4096, 700},  // below threshold, warns
        {"idle", 1024, 900},
    };
    auto first = core.tick(withSband);
    ASSERT_EQ(first.newWarnings.size(), 1u);

    // sband thread has exited; only idle remains this tick.
    std::vector<ThreadStackSample> withoutSband = {
        {"idle", 1024, 900},
    };
    auto second = core.tick(withoutSband);

    EXPECT_TRUE(second.newWarnings.empty());
    EXPECT_TRUE(second.newRecoveries.empty());
    EXPECT_EQ(second.summary.worstThreadName, "idle");
    EXPECT_EQ(second.summary.threadsBelowThreshold, 0u);

    // sband reappears still below threshold: no re-warn (hysteresis survives the gap).
    auto third = core.tick(withSband);
    EXPECT_TRUE(third.newWarnings.empty());
    EXPECT_EQ(third.summary.threadsBelowThreshold, 1u);
}
