#include <gtest/gtest.h>

#include <cstdint>
#include <random>

#include "PROVESFlightControllerReference/Components/Drv/RtcManager/TimeDiscipline.hpp"

using Drv::TimeDiscipline;
using Correction = Drv::TimeDiscipline::Correction;

namespace {
constexpr std::int64_t US_PER_S = 1000000;
}  // namespace

TEST(TimeDisciplineTest, NotSeededReturnsFalse) {
    TimeDiscipline td;
    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    EXPECT_FALSE(td.read(0, seconds, useconds));
    EXPECT_FALSE(td.read(123456, seconds, useconds));
}

TEST(TimeDisciplineTest, SeedThenReadIsRtcPlusElapsed) {
    TimeDiscipline td;
    td.seed(1000, 5 * US_PER_S);  // rtc_s=1000 at uptime=5s -> offset = 1000e6 - 5e6

    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    ASSERT_TRUE(td.read(5 * US_PER_S, seconds, useconds));
    EXPECT_EQ(seconds, 1000U);
    EXPECT_EQ(useconds, 0U);

    ASSERT_TRUE(td.read(5 * US_PER_S + 250000, seconds, useconds));
    EXPECT_EQ(seconds, 1000U);
    EXPECT_EQ(useconds, 250000U);

    ASSERT_TRUE(td.read(6 * US_PER_S + 250000, seconds, useconds));
    EXPECT_EQ(seconds, 1001U);
    EXPECT_EQ(useconds, 250000U);
}

TEST(TimeDisciplineTest, UsecondsAlwaysBelowOneMillion) {
    TimeDiscipline td;
    td.seed(0, 0);

    std::mt19937 rng(12345);
    std::uniform_int_distribution<std::int64_t> step_dist(0, 3000);

    std::int64_t uptime = 0;
    for (int i = 0; i < 1000000; ++i) {
        uptime += step_dist(rng);
        std::uint32_t seconds = 0;
        std::uint32_t useconds = 0;
        ASSERT_TRUE(td.read(uptime, seconds, useconds));
        ASSERT_LT(useconds, 1000000U);
    }
}

TEST(TimeDisciplineTest, SmallBackwardCorrectionIsAbsorbed) {
    TimeDiscipline td;
    td.seed(1000, 0);  // offset = 1000e6

    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    ASSERT_TRUE(td.read(1 * US_PER_S, seconds, useconds));
    EXPECT_EQ(seconds, 1001U);
    std::int64_t previous_reported = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;

    // rtc_s = 1001 at uptime 1s would be offset 1000e6, a -50ms correction from current offset
    // (previous offset was exactly 1000e6, so pick an rtc read that yields -50ms correction)
    Drv::TimeDiscipline::CorrectionResult result = td.correct(1001, 1 * US_PER_S + 50000);
    // new_offset_candidate = 1001e6 - (1e6+50000) = 999950000; old offset = 1000000000
    // correction = 999950000 - 1000000000 = -50000 (-50ms)
    EXPECT_EQ(result.kind, Correction::APPLIED);
    EXPECT_EQ(result.correction_us, -50000);

    ASSERT_TRUE(td.read(1 * US_PER_S + 50000, seconds, useconds));
    std::int64_t reported = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;
    EXPECT_GE(reported, previous_reported);
}

TEST(TimeDisciplineTest, LargeBackwardCorrectionSteps) {
    TimeDiscipline td;
    td.seed(1000, 0);  // offset = 1000e6

    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    ASSERT_TRUE(td.read(0, seconds, useconds));
    std::int64_t before = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;

    // First REJECTED backward correction. Offset stays at the seed value (1000e6), so pick
    // uptime1 close to where the second (forced) correction will land, to isolate the
    // offset's backward jump from ordinary uptime advance between the two reads.
    std::int64_t rtc1 = 1001;
    std::int64_t uptime1 = 2050000;  // correction = 1001e6 - 2050000 - 1000000000 = -1050000
    auto r1 = td.correct(rtc1, uptime1);
    EXPECT_EQ(r1.kind, Correction::REJECTED);
    EXPECT_EQ(r1.correction_us, -1050000);

    // offset unchanged; read should still reflect old (seed) offset
    ASSERT_TRUE(td.read(uptime1, seconds, useconds));
    std::int64_t mid = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;
    EXPECT_GE(mid, before);

    // Second sequential backward correction beyond threshold, consistent with the first
    // (within STEP_THRESHOLD_US) -> STEPPED. The correction math only depends on the unchanged
    // seed offset, not on rtc1/uptime1.
    std::int64_t rtc2 = 1002;
    std::int64_t uptime2 = 3050000;  // correction = 1002e6 - 3050000 - 1000000000 = -1050000
    auto r2 = td.correct(rtc2, uptime2);
    EXPECT_EQ(r2.kind, Correction::STEPPED);
    EXPECT_EQ(r2.correction_us, -1050000);

    ASSERT_TRUE(td.read(uptime2, seconds, useconds));
    std::int64_t after = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;
    EXPECT_LT(after, mid);
}

TEST(TimeDisciplineTest, LargeForwardCorrectionSteps) {
    TimeDiscipline td;
    // Seed with a 0.6s bias error: seed at rtc_s=1000, uptime=0 -> offset=1000e6
    td.seed(1000, 0);

    // First correction one second later at the true edge would give correction 0.
    // Instead simulate a biased seed: true offset should have been 1000.6e6, so the
    // first correct() reveals the +0.6s forward error.
    std::int64_t uptime = 1 * US_PER_S;
    std::int64_t rtc_s = 1001;
    // To get +600000 correction: new_offset = offset + 600000 = 1000600000
    // rtc_s*1e6 - uptime = 1000600000 -> rtc_s*1e6 = 1000600000 + uptime
    // with uptime = 1e6 -> rtc_s*1e6 = 1001600000 -> not integer seconds; adjust uptime instead.
    uptime = 1000000 - 600000;  // 400000
    auto first = td.correct(rtc_s, uptime);
    EXPECT_EQ(first.kind, Correction::REJECTED);
    EXPECT_EQ(first.correction_us, 600000);

    // Second consistent forward correction one second later confirms the step
    auto second = td.correct(rtc_s + 1, uptime + US_PER_S);
    EXPECT_EQ(second.kind, Correction::STEPPED);
    EXPECT_EQ(second.correction_us, 600000);
}

TEST(TimeDisciplineTest, StaleCallbackIsIgnored) {
    TimeDiscipline td;
    td.seed(1000, 0);
    auto first = td.correct(1001, 1 * US_PER_S);
    EXPECT_NE(first.kind, Correction::IGNORED);

    // Same rtc_s again (driver-registration duplicate sample)
    auto stale = td.correct(1001, 1 * US_PER_S + 500);
    EXPECT_EQ(stale.kind, Correction::IGNORED);
    EXPECT_EQ(stale.correction_us, 0);
}

TEST(TimeDisciplineTest, MissedEdgeStillApplies) {
    TimeDiscipline td;
    td.seed(1000, 0);
    // Missed one edge: rtc jumps by 2 seconds
    auto result = td.correct(1002, 2 * US_PER_S);
    EXPECT_TRUE(result.kind == Correction::APPLIED || result.kind == Correction::STEPPED);
}

TEST(TimeDisciplineTest, TimeSetBackwardStepsOnce) {
    TimeDiscipline td;
    td.seed(2000, 0);
    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    ASSERT_TRUE(td.read(0, seconds, useconds));
    EXPECT_EQ(seconds, 2000U);

    // TIME_SET seeds backward
    td.seed(1000, 0);
    ASSERT_TRUE(td.read(0, seconds, useconds));
    EXPECT_EQ(seconds, 1000U);

    // Next correct() with rtc_s = last_rtc_s(1000)+1 should be APPLIED, not IGNORED
    auto result = td.correct(1001, 1 * US_PER_S);
    EXPECT_NE(result.kind, Correction::IGNORED);
}

TEST(TimeDisciplineTest, MonotonicUnderRandomInterleaving) {
    TimeDiscipline td;
    td.seed(1000, 0);

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> op_dist(0, 2);
    std::uniform_int_distribution<std::int64_t> corr_dist(-100000, 100000);
    std::uniform_int_distribution<int> late_gap(1, 20);

    std::int64_t uptime = 0;
    std::int64_t rtc_s = 1000;
    std::int64_t last_reported = -1;
    int steps_since_late = 0;

    for (int i = 0; i < 1000000; ++i) {
        int op = op_dist(rng);
        if (op == 0) {
            uptime += 1000;  // advance uptime a bit
            std::uint32_t seconds = 0;
            std::uint32_t useconds = 0;
            bool ok = td.read(uptime, seconds, useconds);
            ASSERT_TRUE(ok);
            std::int64_t reported = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;
            ASSERT_GE(reported, last_reported);
            last_reported = reported;
        } else {
            std::int64_t edge_uptime = uptime + 1000;
            bool late = (steps_since_late > 50) && (op == 2);
            std::int64_t correction_target = corr_dist(rng);
            if (late) {
                correction_target = 150000;  // > 100ms late correction, single occurrence
                steps_since_late = 0;
            } else {
                steps_since_late++;
                if (correction_target < -100000) {
                    correction_target = -50000;
                }
                if (correction_target > 100000) {
                    correction_target = 50000;
                }
            }
            rtc_s += 1;
            std::int64_t desired_new_offset_candidate = correction_target;
            // We don't track current offset here directly; just drive correct() with
            // rtc_s/uptime chosen to be roughly consistent (edge every ~1s of uptime).
            static_cast<void>(desired_new_offset_candidate);
            td.correct(rtc_s, edge_uptime);
            uptime = edge_uptime;
        }
    }
}

TEST(TimeDisciplineTest, MonotonicUnderSimulatedDrift) {
    TimeDiscipline td;
    td.seed(0, 0);

    std::mt19937 rng(7);
    std::uniform_real_distribution<double> ppm_dist(-50.0, 50.0);

    std::int64_t last_reported = 0;
    std::int64_t true_time_us = 0;
    std::int64_t uptime_us = 0;
    std::int64_t rtc_s = 0;

    // Simulate 24h of 1-second edges
    const int total_seconds = 24 * 3600;
    for (int s = 0; s < total_seconds; ++s) {
        double ppm = ppm_dist(rng);
        // uptime advances by 1s scaled by drift (processor clock runs fast/slow vs RTC)
        double drift_factor = 1.0 + (ppm / 1e6);
        uptime_us += static_cast<std::int64_t>(US_PER_S * drift_factor);
        true_time_us += US_PER_S;
        rtc_s = true_time_us / US_PER_S;

        std::uint32_t seconds = 0;
        std::uint32_t useconds = 0;
        ASSERT_TRUE(td.read(uptime_us, seconds, useconds));
        std::int64_t reported = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;
        ASSERT_GE(reported, last_reported);
        last_reported = reported;

        td.correct(rtc_s, uptime_us);

        ASSERT_TRUE(td.read(uptime_us, seconds, useconds));
        reported = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;
        ASSERT_GE(reported, last_reported);
        last_reported = reported;
    }
}

TEST(TimeDisciplineTest, ThresholdBoundary) {
    {
        TimeDiscipline td;
        td.seed(1000, 0);  // offset = 1000e6
        // correction = -100000: new_offset = 1000e6 - 100000 = 999900000
        // rtc_s*1e6 - uptime = 999900000; rtc_s=1001 -> uptime = 1001e6 - 999900000 = 1100000
        auto r = td.correct(1001, 1100000);
        EXPECT_EQ(r.kind, Correction::APPLIED);
        EXPECT_EQ(r.correction_us, -100000);
    }
    {
        TimeDiscipline td;
        td.seed(1000, 0);
        // correction = -100001: uptime = 1001e6 - (1000e6-100001) = 1100001
        auto r = td.correct(1001, 1100001);
        EXPECT_EQ(r.kind, Correction::REJECTED);
        EXPECT_EQ(r.correction_us, -100001);
    }
    {
        TimeDiscipline td;
        td.seed(1000, 0);
        // correction = +100000: new_offset = 1000e6+100000=1000100000
        // rtc_s=1001 -> uptime = 1001e6 - 1000100000 = 900000
        auto r = td.correct(1001, 900000);
        EXPECT_EQ(r.kind, Correction::APPLIED);
        EXPECT_EQ(r.correction_us, 100000);
    }
    {
        TimeDiscipline td;
        td.seed(1000, 0);
        // correction = +100001: uptime = 1001e6 - 1000100001 = 899999. Out of band, so the
        // first sample needs confirmation.
        auto r = td.correct(1001, 899999);
        EXPECT_EQ(r.kind, Correction::REJECTED);
        EXPECT_EQ(r.correction_us, 100001);
    }
}

TEST(TimeDisciplineTest, CorrectBeforeSeedSeeds) {
    TimeDiscipline td;
    auto result = td.correct(500, 12345);
    EXPECT_EQ(result.kind, Correction::APPLIED);
    EXPECT_EQ(result.correction_us, 0);

    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    EXPECT_TRUE(td.read(12345, seconds, useconds));
    EXPECT_EQ(seconds, 500U);

    // Never STEPPED on the seeding call
    EXPECT_NE(result.kind, Correction::STEPPED);
}

TEST(TimeDisciplineTest, SingleLateCallbackIsRejected) {
    TimeDiscipline td;
    td.seed(1000, 0);  // offset = 1000e6

    // A few on-time edges (correction ~0)
    std::int64_t uptime = 0;
    std::int64_t rtc_s = 1000;
    for (int i = 0; i < 3; ++i) {
        uptime += US_PER_S;
        rtc_s += 1;
        auto r = td.correct(rtc_s, uptime);
        EXPECT_EQ(r.kind, Correction::APPLIED);
    }

    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    ASSERT_TRUE(td.read(uptime, seconds, useconds));
    std::int64_t before_late = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;

    // One callback 150ms late: the edge is measured 150ms later than expected, so uptime
    // advances 1.15s while rtc advances only 1s -> the offset looks like it dropped, a false
    // backward correction.
    uptime += 1150000;
    rtc_s += 1;
    auto late = td.correct(rtc_s, uptime);
    EXPECT_EQ(late.kind, Correction::REJECTED);
    EXPECT_EQ(late.correction_us, -150000);

    ASSERT_TRUE(td.read(uptime, seconds, useconds));
    std::int64_t mid = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;
    EXPECT_GE(mid, before_late);

    // Next on-time edge catches back up (850ms later) and should be APPLIED with a small
    // correction back toward the pre-late offset, since the rejected correction never changed it.
    uptime += 850000;
    rtc_s += 1;
    auto ontime = td.correct(rtc_s, uptime);
    EXPECT_EQ(ontime.kind, Correction::APPLIED);
    EXPECT_LT(ontime.correction_us < 0 ? -ontime.correction_us : ontime.correction_us, 1000);

    ASSERT_TRUE(td.read(uptime, seconds, useconds));
    std::int64_t after = static_cast<std::int64_t>(seconds) * US_PER_S + useconds;
    EXPECT_GE(after, mid);
}

TEST(TimeDisciplineTest, BackwardCountResetsOnGoodCorrection) {
    TimeDiscipline td;
    td.seed(1000, 0);  // offset = 1000e6

    std::int64_t uptime = US_PER_S + 150000;  // 150ms late edge
    std::int64_t rtc_s = 1001;
    auto r1 = td.correct(rtc_s, uptime);
    EXPECT_EQ(r1.kind, Correction::REJECTED);

    // Good correction (catches back up, small forward correction, offset unchanged by r1)
    uptime += 850000;
    rtc_s += 1;
    auto r2 = td.correct(rtc_s, uptime);
    EXPECT_EQ(r2.kind, Correction::APPLIED);

    // Late again -> should be REJECTED again (count was reset by the good correction, not
    // carried over toward a step)
    uptime += US_PER_S + 150000;
    rtc_s += 1;
    auto r3 = td.correct(rtc_s, uptime);
    EXPECT_EQ(r3.kind, Correction::REJECTED);
}

TEST(TimeDisciplineTest, SeedResetsBackwardCount) {
    TimeDiscipline td;
    td.seed(1000, 0);  // offset = 1000e6

    std::int64_t uptime = US_PER_S;
    std::int64_t rtc_s = 1001;
    auto r1 = td.correct(rtc_s, uptime + 150000);
    EXPECT_EQ(r1.kind, Correction::REJECTED);

    // Re-seed (e.g. TIME_SET)
    td.seed(2000, uptime + 150000);

    // Late correction again should be REJECTED, not STEPPED, since backward count was reset
    std::int64_t uptime2 = uptime + 150000 + US_PER_S;
    std::int64_t rtc_s2 = 2001;
    auto r2 = td.correct(rtc_s2, uptime2 + 150000);
    EXPECT_EQ(r2.kind, Correction::REJECTED);
}

namespace {
std::int64_t reportedUs(TimeDiscipline& td, std::int64_t uptime_us) {
    std::uint32_t seconds = 0;
    std::uint32_t useconds = 0;
    EXPECT_TRUE(td.read(uptime_us, seconds, useconds));
    return static_cast<std::int64_t>(seconds) * US_PER_S + useconds;
}
}  // namespace

TEST(TimeDisciplineTest, SingleBogusForwardSampleDoesNotStick) {
    TimeDiscipline td;
    const std::int64_t rtc0 = 1800000000;  // 2027-01-15
    td.seed(rtc0, 0);

    std::int64_t uptime = US_PER_S;
    auto r0 = td.correct(rtc0 + 1, uptime);
    EXPECT_EQ(r0.kind, Correction::APPLIED);

    // One corrupted read far in the future (e.g. year 2099)
    const std::int64_t bogus = 4070908800;  // 2099-01-01
    uptime += US_PER_S;
    auto bad = td.correct(bogus, uptime);
    EXPECT_EQ(bad.kind, Correction::REJECTED);
    EXPECT_GT(bad.correction_us, static_cast<std::int64_t>(TimeDiscipline::STEP_THRESHOLD_US));

    // Time did not jump
    EXPECT_LT(reportedUs(td, uptime), (rtc0 + 3) * US_PER_S);

    // Later good samples are not ignored and keep time correct
    for (int i = 3; i < 10; ++i) {
        uptime += US_PER_S;
        auto good = td.correct(rtc0 + i, uptime);
        EXPECT_EQ(good.kind, Correction::APPLIED) << "i=" << i;
        EXPECT_EQ(good.correction_us, 0) << "i=" << i;
        EXPECT_EQ(reportedUs(td, uptime), (rtc0 + i) * US_PER_S);
    }
}

TEST(TimeDisciplineTest, InconsistentForwardSamplesDoNotStep) {
    TimeDiscipline td;
    td.seed(1000, 0);

    // Two out-of-band forward samples that disagree by more than STEP_THRESHOLD_US
    auto r1 = td.correct(5000, US_PER_S);  // correction = +3999 s
    EXPECT_EQ(r1.kind, Correction::REJECTED);
    auto r2 = td.correct(9000, 2 * US_PER_S);  // correction = +7998 s
    EXPECT_EQ(r2.kind, Correction::REJECTED);

    EXPECT_EQ(reportedUs(td, 2 * US_PER_S), 1002 * US_PER_S);

    // Good sample is applied
    auto r3 = td.correct(1003, 3 * US_PER_S);
    EXPECT_EQ(r3.kind, Correction::APPLIED);
    EXPECT_EQ(r3.correction_us, 0);
}

TEST(TimeDisciplineTest, DuplicateOfPendingSampleDoesNotConfirm) {
    TimeDiscipline td;
    td.seed(1000, 0);

    // Same out-of-band rtc_s seen twice a few ms apart must not confirm a step
    auto r1 = td.correct(1001, 400000);  // +600 ms
    EXPECT_EQ(r1.kind, Correction::REJECTED);
    auto r2 = td.correct(1001, 402000);  // +598 ms, same rtc_s
    EXPECT_NE(r2.kind, Correction::STEPPED);

    // Next edge confirms
    auto r3 = td.correct(1002, 1400000);
    EXPECT_EQ(r3.kind, Correction::STEPPED);
    EXPECT_EQ(r3.correction_us, 600000);
}

TEST(TimeDisciplineTest, BootSeedTruncationConverges) {
    TimeDiscipline td;
    // Boot at true time 1000.7 s: RTC reads 1000, so the seed is 0.7 s behind
    const std::int64_t boot_uptime = 5 * US_PER_S;
    td.seed(1000, boot_uptime);

    // Driver-registration duplicate sample (same seconds) is ignored
    auto dup = td.correct(1000, boot_uptime + 1000);
    EXPECT_EQ(dup.kind, Correction::IGNORED);

    // First real edge (1001 at true 1001.0, i.e. 0.3 s after boot): unconfirmed
    std::int64_t uptime = boot_uptime + 300000;
    auto e1 = td.correct(1001, uptime);
    EXPECT_EQ(e1.kind, Correction::REJECTED);
    EXPECT_EQ(e1.correction_us, 700000);

    // Second edge confirms
    uptime += US_PER_S;
    auto e2 = td.correct(1002, uptime);
    EXPECT_EQ(e2.kind, Correction::STEPPED);
    EXPECT_EQ(e2.correction_us, 700000);
    EXPECT_EQ(reportedUs(td, uptime), 1002 * US_PER_S);

    // Steady state
    uptime += US_PER_S;
    auto e3 = td.correct(1003, uptime);
    EXPECT_EQ(e3.kind, Correction::APPLIED);
    EXPECT_EQ(e3.correction_us, 0);
}

TEST(TimeDisciplineTest, InconsistentBackwardSamplesDoNotStep) {
    TimeDiscipline td;
    td.seed(1000, 0);  // offset = 1000e6

    // Late callback (-150 ms), then an unrelated large backward sample (-1.05 s)
    auto r1 = td.correct(1001, 1150000);
    EXPECT_EQ(r1.kind, Correction::REJECTED);
    auto r2 = td.correct(1002, 3050000);
    EXPECT_EQ(r2.kind, Correction::REJECTED);

    // Next consistent sample confirms the -1.05 s step
    auto r3 = td.correct(1003, 4050000);
    EXPECT_EQ(r3.kind, Correction::STEPPED);
    EXPECT_EQ(r3.correction_us, -1050000);
}

TEST(TimeDisciplineTest, PlausibleRtcSecondsRange) {
    // RV3028 represents years 2000 to 2099
    EXPECT_FALSE(TimeDiscipline::isPlausibleRtcSeconds(0));
    EXPECT_FALSE(TimeDiscipline::isPlausibleRtcSeconds(-1));
    EXPECT_FALSE(TimeDiscipline::isPlausibleRtcSeconds(946684799));  // 1999-12-31T23:59:59Z
    EXPECT_TRUE(TimeDiscipline::isPlausibleRtcSeconds(946684800));   // 2000-01-01T00:00:00Z
    EXPECT_TRUE(TimeDiscipline::isPlausibleRtcSeconds(1800000000));
    EXPECT_TRUE(TimeDiscipline::isPlausibleRtcSeconds(4102444799));   // 2099-12-31T23:59:59Z
    EXPECT_FALSE(TimeDiscipline::isPlausibleRtcSeconds(4102444800));  // 2100-01-01T00:00:00Z
    EXPECT_EQ(static_cast<std::int64_t>(TimeDiscipline::RTC_MIN_S), 946684800);
    EXPECT_EQ(static_cast<std::int64_t>(TimeDiscipline::RTC_MAX_S), 4102444799);
}
