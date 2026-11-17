#include <gtest/gtest.h>

#include <cstdint>
#include <random>

#include "PROVESFlightControllerReference/Components/Drv/RtcManager/TimeDiscipline.hpp"

using Drv::TimeDiscipline;
using Correction = Drv::TimeDiscipline::Correction;

namespace {
constexpr std::int64_t US_PER_S = TimeDiscipline::US_PER_S;
constexpr std::int64_t STEP_US = TimeDiscipline::STEP_THRESHOLD_US;

//! Uptime at the n-th edge after seed(rtc, 0) that gives correction c
std::int64_t uptimeFor(std::int64_t n, std::int64_t c) {
    return n * US_PER_S - c;
}

//! Reported time in microseconds at uptime_us; checks the microseconds field is below one million
std::int64_t reportedUs(TimeDiscipline& td, std::int64_t uptime_us) {
    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    EXPECT_TRUE(td.read(uptime_us, seconds, useconds));
    EXPECT_LT(useconds, 1000000U);
    return static_cast<std::int64_t>(seconds) * US_PER_S + useconds;
}
}  // namespace

TEST(TimeDisciplineTest, NotSeededReturnsFalse) {
    TimeDiscipline td;
    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    EXPECT_FALSE(td.read(123456, seconds, useconds));
}

TEST(TimeDisciplineTest, SeedThenReadIsRtcPlusElapsed) {
    TimeDiscipline td;
    td.seed(1000, 5 * US_PER_S);
    EXPECT_EQ(reportedUs(td, 5 * US_PER_S), 1000 * US_PER_S);
    EXPECT_EQ(reportedUs(td, 6 * US_PER_S + 250000), 1001 * US_PER_S + 250000);
}

TEST(TimeDisciplineTest, CorrectBeforeSeedSeeds) {
    TimeDiscipline td;
    auto result = td.correct(500, 12345);
    EXPECT_EQ(result.kind, Correction::APPLIED);
    EXPECT_EQ(result.correction_us, 0);
    EXPECT_EQ(reportedUs(td, 12345), 500 * US_PER_S);
}

TEST(TimeDisciplineTest, ThresholdBoundary) {
    for (const std::int64_t c : {-STEP_US, -STEP_US - 1, STEP_US, STEP_US + 1}) {
        TimeDiscipline td;
        td.seed(1000, 0);
        const std::int64_t before = reportedUs(td, uptimeFor(1, c));
        auto r = td.correct(1001, uptimeFor(1, c));
        EXPECT_EQ(r.kind, (c >= -STEP_US && c <= STEP_US) ? Correction::APPLIED : Correction::REJECTED) << c;
        EXPECT_EQ(r.correction_us, c);
        EXPECT_GE(reportedUs(td, uptimeFor(1, c)), before) << c;
    }
}

TEST(TimeDisciplineTest, TwoConsistentLargeCorrectionsStep) {
    for (const std::int64_t c : {600000, -1050000}) {
        TimeDiscipline td;
        td.seed(1000, 0);
        EXPECT_EQ(td.correct(1001, uptimeFor(1, c)).kind, Correction::REJECTED) << c;
        const std::int64_t before = reportedUs(td, uptimeFor(2, c));
        auto r = td.correct(1002, uptimeFor(2, c));
        EXPECT_EQ(r.kind, Correction::STEPPED) << c;
        EXPECT_EQ(r.correction_us, c);
        EXPECT_EQ(reportedUs(td, uptimeFor(2, c)) - before, c);
    }
}

TEST(TimeDisciplineTest, InconsistentLargeCorrectionsDoNotStep) {
    TimeDiscipline td;
    td.seed(1000, 0);
    EXPECT_EQ(td.correct(5000, US_PER_S).kind, Correction::REJECTED);               // +3999 s
    EXPECT_EQ(td.correct(9000, 2 * US_PER_S).kind, Correction::REJECTED);           // +7998 s
    EXPECT_EQ(td.correct(1003, 4 * US_PER_S + 150000).kind, Correction::REJECTED);  // -1.15 s
    EXPECT_EQ(reportedUs(td, 4 * US_PER_S), 1004 * US_PER_S);
    EXPECT_EQ(td.correct(1004, 4 * US_PER_S).kind, Correction::APPLIED);
}

TEST(TimeDisciplineTest, StaleSampleIsIgnored) {
    TimeDiscipline td;
    td.seed(1000, 0);
    EXPECT_EQ(td.correct(1000, 1000).kind, Correction::IGNORED);
    EXPECT_EQ(td.correct(1001, US_PER_S).kind, Correction::APPLIED);
    auto stale = td.correct(1001, US_PER_S + 500);
    EXPECT_EQ(stale.kind, Correction::IGNORED);
    EXPECT_EQ(stale.correction_us, 0);
    EXPECT_EQ(td.correct(1003, 3 * US_PER_S).kind, Correction::APPLIED);  // missed edge
}

TEST(TimeDisciplineTest, DuplicateOfPendingSampleDoesNotConfirm) {
    TimeDiscipline td;
    td.seed(1000, 0);
    EXPECT_EQ(td.correct(1001, 400000).kind, Correction::REJECTED);  // +600 ms
    EXPECT_EQ(td.correct(1001, 402000).kind, Correction::REJECTED);  // +598 ms, same rtc_s
    EXPECT_EQ(td.correct(1002, 1400000).kind, Correction::STEPPED);  // +600 ms
}

TEST(TimeDisciplineTest, SingleLateCallbackIsRejected) {
    TimeDiscipline td;
    td.seed(1000, 0);
    const std::int64_t before = reportedUs(td, 2 * US_PER_S + 150000);

    // Each late callback gives a -150 ms correction; an on-time edge between them resets the count
    EXPECT_EQ(td.correct(1002, 2 * US_PER_S + 150000).kind, Correction::REJECTED);
    EXPECT_GE(reportedUs(td, 2 * US_PER_S + 150000), before);
    EXPECT_EQ(td.correct(1003, 3 * US_PER_S).kind, Correction::APPLIED);
    EXPECT_EQ(td.correct(1004, 4 * US_PER_S + 150000).kind, Correction::REJECTED);
}

TEST(TimeDisciplineTest, SeedResetsPendingCount) {
    TimeDiscipline td;
    td.seed(1000, 0);
    EXPECT_EQ(td.correct(1001, US_PER_S + 150000).kind, Correction::REJECTED);
    td.seed(2000, US_PER_S);
    // Would be STEPPED if seed() kept the pending count
    EXPECT_EQ(td.correct(2001, 2 * US_PER_S + 150000).kind, Correction::REJECTED);
}

TEST(TimeDisciplineTest, TimeSetBackwardSeedsOnce) {
    TimeDiscipline td;
    td.seed(2000, 0);
    EXPECT_EQ(reportedUs(td, 0), 2000 * US_PER_S);
    td.seed(1000, 0);
    EXPECT_EQ(reportedUs(td, 0), 1000 * US_PER_S);
    EXPECT_EQ(td.correct(1001, US_PER_S).kind, Correction::APPLIED);
}

TEST(TimeDisciplineTest, SingleBogusForwardSampleDoesNotStick) {
    TimeDiscipline td;
    const std::int64_t rtc0 = 1800000000;  // 2027-01-15
    td.seed(rtc0, 0);

    auto bad = td.correct(4070908800, US_PER_S);  // 2099-01-01
    EXPECT_EQ(bad.kind, Correction::REJECTED);
    EXPECT_EQ(reportedUs(td, US_PER_S), (rtc0 + 1) * US_PER_S);

    for (std::int64_t i = 2; i < 10; ++i) {
        auto good = td.correct(rtc0 + i, i * US_PER_S);
        EXPECT_EQ(good.kind, Correction::APPLIED) << i;
        EXPECT_EQ(good.correction_us, 0) << i;
    }
}

TEST(TimeDisciplineTest, MonotonicAndBoundedUnderRandomInterleaving) {
    // Reads at random uptimes interleaved with edges whose callback delay is random in
    // [0, 100 ms]; every 50th edge is one 250 ms late callback. RTC time equals uptime.
    TimeDiscipline td;
    td.seed(0, 0);
    std::mt19937 rng(42);
    std::uniform_int_distribution<std::int64_t> delay_dist(0, STEP_US);
    std::uniform_int_distribution<std::int64_t> read_gap_dist(0, 200000);

    std::int64_t uptime = 0;
    std::int64_t edge_s = 1;
    std::int64_t callback_us = US_PER_S + delay_dist(rng);
    std::int64_t last_reported = 0;
    for (int i = 0; i < 1000000; ++i) {
        uptime += read_gap_dist(rng);
        if (uptime >= callback_us) {
            ASSERT_NE(td.correct(edge_s, callback_us).kind, Correction::STEPPED);
            ++edge_s;
            callback_us = edge_s * US_PER_S + ((edge_s % 50 == 0) ? 250000 : delay_dist(rng));
        }
        const std::int64_t reported = reportedUs(td, uptime);
        ASSERT_GE(reported, last_reported);
        ASSERT_LE(reported, uptime);
        ASSERT_GE(reported, uptime - STEP_US);
        last_reported = reported;
    }
}

TEST(TimeDisciplineTest, MonotonicUnderSimulatedDrift) {
    // 24 h of edges with uptime drifting by a random +-50 ppm each second
    TimeDiscipline td;
    td.seed(0, 0);
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> ppm_dist(-50.0, 50.0);

    std::int64_t uptime = 0;
    std::int64_t last_reported = 0;
    for (std::int64_t rtc_s = 1; rtc_s <= 24 * 3600; ++rtc_s) {
        uptime += static_cast<std::int64_t>(US_PER_S * (1.0 + ppm_dist(rng) / 1e6));
        ASSERT_GE(reportedUs(td, uptime), last_reported);
        ASSERT_EQ(td.correct(rtc_s, uptime).kind, Correction::APPLIED);
        last_reported = reportedUs(td, uptime);
    }
}

TEST(TimeDisciplineTest, PlausibleRtcSecondsRange) {
    EXPECT_FALSE(TimeDiscipline::isPlausibleRtcSeconds(-1));
    EXPECT_FALSE(TimeDiscipline::isPlausibleRtcSeconds(946684799));   // 1999-12-31T23:59:59Z
    EXPECT_TRUE(TimeDiscipline::isPlausibleRtcSeconds(946684800));    // 2000-01-01T00:00:00Z
    EXPECT_TRUE(TimeDiscipline::isPlausibleRtcSeconds(4102444799));   // 2099-12-31T23:59:59Z
    EXPECT_FALSE(TimeDiscipline::isPlausibleRtcSeconds(4102444800));  // 2100-01-01T00:00:00Z
}
