#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/StackMonitor/StackMonitorCore.hpp"

using Components::STACK_MONITOR_MAX_THREADS;
using Components::StackMonitorCore;
using Components::StackMonitorTickResult;
using Components::ThreadStackSampleSet;

// Warn when a thread's free stack drops below 20% of its own size.
const std::uint32_t WARN_THRESHOLD_PERCENT = 20;

TEST(StackMonitorCoreTest, SummarizesWorstThreadFromSampleSet) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    ThreadStackSampleSet samples;
    samples.clear();
    samples.add("idle", 1024, 900);       // 12% used, healthy
    samples.add("sband", 4096, 1024);     // 75% used, worst of the three
    samples.add("watchdog", 2048, 1800);  // ~12% used, healthy

    StackMonitorTickResult result;
    core.tick(samples, result);

    EXPECT_STREQ(result.summary.worstThreadName, "sband");
    EXPECT_EQ(result.summary.worstThreadFreeBytes, 1024u);
    EXPECT_EQ(result.summary.worstThreadUsedPercent, 75u);
    EXPECT_EQ(result.summary.threadsBelowThreshold, 0u);
}

TEST(StackMonitorCoreTest, WarnsWhenThreadCrossesBelowThreshold) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    // sband: 4096 total, 700 free => ~17% free, below the 20% threshold.
    ThreadStackSampleSet samples;
    samples.clear();
    samples.add("sband", 4096, 700);

    StackMonitorTickResult result;
    core.tick(samples, result);

    EXPECT_EQ(result.summary.threadsBelowThreshold, 1u);
    ASSERT_EQ(result.warningCount, 1u);
    EXPECT_STREQ(result.newWarnings[0].name, "sband");
    EXPECT_EQ(result.newWarnings[0].freeBytes, 700u);
    EXPECT_EQ(result.newWarnings[0].sizeBytes, 4096u);
    EXPECT_EQ(result.recoveryCount, 0u);
}

TEST(StackMonitorCoreTest, DoesNotReWarnWhileStillBelowThreshold) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    ThreadStackSampleSet samples;
    samples.clear();
    samples.add("sband", 4096, 700);  // below threshold

    StackMonitorTickResult result;
    core.tick(samples, result);
    ASSERT_EQ(result.warningCount, 1u);

    // Still below threshold on the next tick: no new warning (hysteresis).
    core.tick(samples, result);
    EXPECT_EQ(result.warningCount, 0u);
    EXPECT_EQ(result.summary.threadsBelowThreshold, 1u);
}

TEST(StackMonitorCoreTest, ClearsWhenThreadRecoversAboveThreshold) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    ThreadStackSampleSet low;
    low.clear();
    low.add("sband", 4096, 700);  // ~17% free, below threshold

    StackMonitorTickResult result;
    core.tick(low, result);
    ASSERT_EQ(result.warningCount, 1u);

    ThreadStackSampleSet recovered;
    recovered.clear();
    recovered.add("sband", 4096, 2048);  // 50% free, back above threshold

    core.tick(recovered, result);

    EXPECT_EQ(result.warningCount, 0u);
    ASSERT_EQ(result.recoveryCount, 1u);
    EXPECT_STREQ(result.newRecoveries[0].name, "sband");
    EXPECT_EQ(result.summary.threadsBelowThreshold, 0u);

    // Crossing below threshold again after recovery re-arms the warning.
    core.tick(low, result);
    ASSERT_EQ(result.warningCount, 1u);
    EXPECT_STREQ(result.newWarnings[0].name, "sband");
}

TEST(StackMonitorCoreTest, EmptySampleSetProducesNoCrashAndNeutralSummary) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    ThreadStackSampleSet samples;
    samples.clear();

    StackMonitorTickResult result;
    core.tick(samples, result);

    EXPECT_STREQ(result.summary.worstThreadName, "");
    EXPECT_EQ(result.summary.worstThreadFreeBytes, 0u);
    EXPECT_EQ(result.summary.threadsBelowThreshold, 0u);
    EXPECT_EQ(result.warningCount, 0u);
    EXPECT_EQ(result.recoveryCount, 0u);
}

TEST(StackMonitorCoreTest, ThreadDisappearingBetweenTicksDoesNotAssert) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    ThreadStackSampleSet withSband;
    withSband.clear();
    withSband.add("sband", 4096, 700);  // below threshold, warns
    withSband.add("idle", 1024, 900);

    StackMonitorTickResult result;
    core.tick(withSband, result);
    ASSERT_EQ(result.warningCount, 1u);

    // sband thread has exited; only idle remains this tick.
    ThreadStackSampleSet withoutSband;
    withoutSband.clear();
    withoutSband.add("idle", 1024, 900);

    core.tick(withoutSband, result);

    EXPECT_EQ(result.warningCount, 0u);
    EXPECT_EQ(result.recoveryCount, 0u);
    EXPECT_STREQ(result.summary.worstThreadName, "idle");
    EXPECT_EQ(result.summary.threadsBelowThreshold, 0u);

    // sband reappears still below threshold: no re-warn (hysteresis survives the gap).
    core.tick(withSband, result);
    EXPECT_EQ(result.warningCount, 0u);
    EXPECT_EQ(result.summary.threadsBelowThreshold, 1u);
}

TEST(StackMonitorCoreTest, MoreThreadsThanCapacitySetsOverflowFlag) {
    StackMonitorCore core(WARN_THRESHOLD_PERCENT);

    ThreadStackSampleSet samples;
    samples.clear();

    // Fill to capacity: every add succeeds, no overflow yet.
    char name[16];
    for (std::uint32_t i = 0; i < STACK_MONITOR_MAX_THREADS; i++) {
        (void)snprintf(name, sizeof(name), "thread%u", i);
        EXPECT_TRUE(samples.add(name, 4096, 2048));
    }
    EXPECT_EQ(samples.count, STACK_MONITOR_MAX_THREADS);
    EXPECT_FALSE(samples.overflowed);

    // One past capacity: the sample is dropped and the flag is raised.
    EXPECT_FALSE(samples.add("straw", 4096, 2048));
    EXPECT_EQ(samples.count, STACK_MONITOR_MAX_THREADS);
    EXPECT_TRUE(samples.overflowed);

    // The overflow is surfaced in the tick summary (no silent truncation).
    StackMonitorTickResult result;
    core.tick(samples, result);
    EXPECT_TRUE(result.summary.overflowed);

    // A subsequent in-capacity tick reports clean again.
    samples.clear();
    samples.add("idle", 1024, 900);
    core.tick(samples, result);
    EXPECT_FALSE(result.summary.overflowed);
}
