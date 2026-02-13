#include <gtest/gtest.h>

#include <array>
#include <chrono>

#include "PROVESFlightControllerReference/Components/DetumbleManager/BDot.hpp"

using Components::BDot;

// Standard sampling period for tests (100 Hz = 10 ms = 10000 us)
const std::chrono::microseconds SAMPLING_PERIOD_US(10000);

TEST(BDotTest, SamplingCompleteAndTimeDelta) {
    BDot bdot;

    // Configure with arbitrary but valid parameters
    const double gain = 1.0;
    bdot.configure(gain, SAMPLING_PERIOD_US);

    // Add 5 samples with increasing timestamps
    for (std::size_t i = 0; i < 5; ++i) {
        std::array<double, 3> b_field = {static_cast<double>(i), 0.0, 0.0};
        bdot.addSample(b_field, SAMPLING_PERIOD_US * static_cast<long long>(i));
    }

    EXPECT_TRUE(bdot.samplingComplete());
    EXPECT_EQ(bdot.getTimeBetweenSamples(), SAMPLING_PERIOD_US * 4);
}

TEST(BDotTest, MagneticMomentLinearXAxis) {
    BDot bdot;

    const double gain = 2.0;
    bdot.configure(gain, SAMPLING_PERIOD_US);

    const double slope_x = 10.0;  // dB/dt in G/s
    const double dt_seconds = SAMPLING_PERIOD_US.count() / 1e6;

    // Create 5 samples forming a linear ramp in X
    for (std::size_t i = 0; i < 5; ++i) {
        double bx = slope_x * dt_seconds * static_cast<double>(i);
        std::array<double, 3> b_field = {bx, 0.0, 0.0};
        bdot.addSample(b_field, SAMPLING_PERIOD_US * static_cast<long long>(i));
    }

    auto moment = bdot.getMagneticMoment();

    // For a linear ramp, the 5-point central difference should recover the slope
    double expected_bdot_x = slope_x;
    double expected_moment_x = -gain * expected_bdot_x;

    EXPECT_NEAR(moment[0], expected_moment_x, 1e-6);
    EXPECT_NEAR(moment[1], 0.0, 1e-6);
    EXPECT_NEAR(moment[2], 0.0, 1e-6);
}

TEST(BDotTest, MagneticMomentLinearMultiAxis) {
    BDot bdot;

    const double gain = -1.5;
    bdot.configure(gain, SAMPLING_PERIOD_US);

    const double slope_x = 5.0;
    const double slope_y = -3.0;
    const double slope_z = 2.0;
    const double dt_seconds = SAMPLING_PERIOD_US.count() / 1e6;

    // Create 5 samples forming independent linear ramps on each axis
    for (std::size_t i = 0; i < 5; ++i) {
        double t_index = static_cast<double>(i);
        std::array<double, 3> b_field = {
            slope_x * dt_seconds * t_index,
            slope_y * dt_seconds * t_index,
            slope_z * dt_seconds * t_index,
        };
        bdot.addSample(b_field, SAMPLING_PERIOD_US * static_cast<long long>(i));
    }

    auto moment = bdot.getMagneticMoment();

    double expected_bdot_x = slope_x;
    double expected_bdot_y = slope_y;
    double expected_bdot_z = slope_z;

    EXPECT_NEAR(moment[0], -gain * expected_bdot_x, 1e-6);
    EXPECT_NEAR(moment[1], -gain * expected_bdot_y, 1e-6);
    EXPECT_NEAR(moment[2], -gain * expected_bdot_z, 1e-6);
}

TEST(BDotTest, EmptySampleSetCanBeReused) {
    BDot bdot;

    const double gain = 1.0;
    bdot.configure(gain, SAMPLING_PERIOD_US);

    // First fill
    for (std::size_t i = 0; i < 5; ++i) {
        std::array<double, 3> b_field = {static_cast<double>(i), 0.0, 0.0};
        bdot.addSample(b_field, SAMPLING_PERIOD_US * static_cast<long long>(i));
    }
    EXPECT_TRUE(bdot.samplingComplete());

    // Clear and ensure we can start over
    bdot.emptySampleSet();

    for (std::size_t i = 0; i < 5; ++i) {
        std::array<double, 3> b_field = {static_cast<double>(i + 10), 0.0, 0.0};
        bdot.addSample(b_field, SAMPLING_PERIOD_US * static_cast<long long>(i));
    }

    EXPECT_TRUE(bdot.samplingComplete());
}
