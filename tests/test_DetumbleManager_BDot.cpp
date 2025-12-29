#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>

#include "FprimeZephyrReference/Components/DetumbleManager/BDot.hpp"

using Components::BDot;

// Define a standard sampling period for tests (e.g., 100Hz = 10ms = 10000us)
const std::chrono::microseconds SAMPLING_PERIOD_US(10000);

TEST(BDotTest, FirstReadingReturnsZeroAndEAGAIN) {
    BDot bdot;
    std::array<double, 3> b_field = {1.0, 2.0, 3.0};

    // First reading always fails time delta check (too large/undefined relative to epoch)
    auto result = bdot.getDipoleMoment(b_field, 100, 0, -1.0, SAMPLING_PERIOD_US);

    EXPECT_EQ(result[0], 0.0);
    EXPECT_EQ(result[1], 0.0);
    EXPECT_EQ(result[2], 0.0);
    EXPECT_EQ(errno, EAGAIN);
}

TEST(BDotTest, ReadingTooFastReturnsZeroAndEINVAL) {
    BDot bdot;
    std::array<double, 3> b1 = {1.0, 0.0, 0.0};

    // Prime the state (returns EAGAIN, but updates internal state)
    bdot.getDipoleMoment(b1, 100, 0, -1.0, SAMPLING_PERIOD_US);

    // Second reading 5ms later (limit is 10ms)
    auto result = bdot.getDipoleMoment(b1, 100, 5000, -1.0, SAMPLING_PERIOD_US);

    EXPECT_EQ(result[0], 0.0);
    EXPECT_EQ(errno, EINVAL);
}

TEST(BDotTest, ReadingTooSlowReturnsZeroAndEAGAIN) {
    BDot bdot;
    std::array<double, 3> b1 = {1.0, 0.0, 0.0};

    // Prime the state
    bdot.getDipoleMoment(b1, 100, 0, -1.0, SAMPLING_PERIOD_US);

    // Second reading 700ms later (limit is 600ms)
    auto result = bdot.getDipoleMoment(b1, 100, 700000, -1.0, SAMPLING_PERIOD_US);

    EXPECT_EQ(result[0], 0.0);
    EXPECT_EQ(errno, EAGAIN);
}

TEST(BDotTest, ValidCalculationXAxis) {
    BDot bdot;
    double gain = -1000.0;

    // t0: B = {10, 0, 0}
    bdot.getDipoleMoment({10.0, 0.0, 0.0}, 100, 0, gain, SAMPLING_PERIOD_US);

    // t1: t0 + 0.1s. B = {15, 0, 0}
    // dt = 0.1s
    // dB/dt = (15-10)/0.1 = 50
    // |B| = 15
    // m = gain * (dB/dt) / |B|
    // m_x = -1000 * 50 / 15 = -3333.333

    auto result = bdot.getDipoleMoment({15.0, 0.0, 0.0}, 100, 100000, gain, SAMPLING_PERIOD_US);

    EXPECT_EQ(errno, 0);
    EXPECT_NEAR(result[0], -3333.333333, 0.001);
    EXPECT_NEAR(result[1], 0.0, 0.001);
    EXPECT_NEAR(result[2], 0.0, 0.001);
}

TEST(BDotTest, ValidCalculationMultiAxis) {
    BDot bdot;
    double gain = 1.0;  // Positive gain for simplicity

    // t0: B = {10, 10, 10}
    bdot.getDipoleMoment({10.0, 10.0, 10.0}, 100, 0, gain, SAMPLING_PERIOD_US);

    // t1: t0 + 0.1s. B = {12, 8, 10}
    // dt = 0.1
    // dB/dt = { (12-10)/0.1, (8-10)/0.1, (10-10)/0.1 } = { 20, -20, 0 }
    // |B| = sqrt(12^2 + 8^2 + 10^2) = sqrt(144 + 64 + 100) = sqrt(308) ~= 17.5499

    auto result = bdot.getDipoleMoment({12.0, 8.0, 10.0}, 100, 100000, gain, SAMPLING_PERIOD_US);

    double mag = std::sqrt(12 * 12 + 8 * 8 + 10 * 10);
    double expected_x = 1.0 * 20.0 / mag;
    double expected_y = 1.0 * -20.0 / mag;
    double expected_z = 0.0;

    EXPECT_EQ(errno, 0);
    EXPECT_NEAR(result[0], expected_x, 0.001);
    EXPECT_NEAR(result[1], expected_y, 0.001);
    EXPECT_NEAR(result[2], expected_z, 0.001);
}

TEST(BDotTest, SmallMagnitudeError) {
    BDot bdot;
    bdot.getDipoleMoment({0.0, 0.0, 0.0}, 100, 0, -1.0, SAMPLING_PERIOD_US);

    // Magnitude < 1e-6
    auto result = bdot.getDipoleMoment({1e-7, 0.0, 0.0}, 100, 100000, -1.0, SAMPLING_PERIOD_US);

    EXPECT_EQ(result[0], 0.0);
    EXPECT_EQ(errno, EDOM);
}
