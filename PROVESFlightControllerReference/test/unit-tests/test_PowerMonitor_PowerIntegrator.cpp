#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/PowerMonitor/PowerIntegrator.hpp"

using Components::PowerIntegrator;

namespace {

// Energy in mWh of a constant power over a duration
double expected_mWh(double powerW, double duration_s) {
    return powerW * duration_s / 3600.0 * 1000.0;
}

}  // namespace

TEST(PowerIntegratorTest, StartsAtZero) {
    PowerIntegrator integrator;

    EXPECT_FLOAT_EQ(integrator.getConsumption_mWh(), 0.0f);
    EXPECT_FLOAT_EQ(integrator.getGeneration_mWh(), 0.0f);
}

TEST(PowerIntegratorTest, FirstUpdateOnlyEstablishesTimeBase) {
    PowerIntegrator integrator;

    integrator.update(100.0, 1.0, 1.0);

    EXPECT_FLOAT_EQ(integrator.getConsumption_mWh(), 0.0f);
    EXPECT_FLOAT_EQ(integrator.getGeneration_mWh(), 0.0f);
}

TEST(PowerIntegratorTest, TimeBaseAtZeroSeconds) {
    PowerIntegrator integrator;

    // A time base of 0 s must not be mistaken for "uninitialized"
    integrator.update(0.0, 3.6, 3.6);
    integrator.update(1.0, 3.6, 3.6);

    EXPECT_NEAR(integrator.getConsumption_mWh(), 1.0, 1e-6);
    EXPECT_NEAR(integrator.getGeneration_mWh(), 1.0, 1e-6);
}

// Regression test for issue #526: generation shared a timestamp with consumption and was credited only with
// the time between the two updates instead of the full tick period.
TEST(PowerIntegratorTest, BothTotalsCreditedWithFullTickPeriod) {
    PowerIntegrator integrator;
    const double consumedW = 1.26;
    const double generatedW = 0.71;
    const int ticks = 3600;

    for (int i = 0; i <= ticks; i++) {
        integrator.update(1000.0 + i, consumedW, generatedW);
    }

    EXPECT_NEAR(integrator.getConsumption_mWh(), expected_mWh(consumedW, ticks), 1e-3);
    EXPECT_NEAR(integrator.getGeneration_mWh(), expected_mWh(generatedW, ticks), 1e-3);
}

TEST(PowerIntegratorTest, NonUniformTickPeriod) {
    PowerIntegrator integrator;

    integrator.update(10.0, 2.0, 1.0);
    integrator.update(10.5, 2.0, 1.0);
    integrator.update(12.0, 2.0, 1.0);

    EXPECT_NEAR(integrator.getConsumption_mWh(), expected_mWh(2.0, 2.0), 1e-6);
    EXPECT_NEAR(integrator.getGeneration_mWh(), expected_mWh(1.0, 2.0), 1e-6);
}

TEST(PowerIntegratorTest, InvalidPowerSkipsOnlyThatTotal) {
    PowerIntegrator integrator;

    integrator.update(0.0, 1.0, 1.0);
    integrator.update(1.0, -1.0, 1.0);    // Invalid consumption
    integrator.update(2.0, 1.0, 1001.0);  // Invalid generation

    EXPECT_NEAR(integrator.getConsumption_mWh(), expected_mWh(1.0, 1.0), 1e-6);
    EXPECT_NEAR(integrator.getGeneration_mWh(), expected_mWh(1.0, 1.0), 1e-6);
}

TEST(PowerIntegratorTest, TimeJumpsAreNotIntegrated) {
    PowerIntegrator integrator;

    integrator.update(0.0, 1.0, 1.0);
    integrator.update(1.0, 1.0, 1.0);    // +1 s
    integrator.update(100.0, 1.0, 1.0);  // Forward jump
    integrator.update(50.0, 1.0, 1.0);   // Backward jump
    integrator.update(51.0, 1.0, 1.0);   // +1 s from the new time base

    EXPECT_NEAR(integrator.getConsumption_mWh(), expected_mWh(1.0, 2.0), 1e-6);
    EXPECT_NEAR(integrator.getGeneration_mWh(), expected_mWh(1.0, 2.0), 1e-6);
}

TEST(PowerIntegratorTest, ResetsAreIndependent) {
    PowerIntegrator integrator;

    integrator.update(0.0, 1.0, 1.0);
    integrator.update(1.0, 1.0, 1.0);

    integrator.resetConsumption();
    EXPECT_FLOAT_EQ(integrator.getConsumption_mWh(), 0.0f);
    EXPECT_NEAR(integrator.getGeneration_mWh(), expected_mWh(1.0, 1.0), 1e-6);

    integrator.update(2.0, 1.0, 1.0);
    EXPECT_NEAR(integrator.getConsumption_mWh(), expected_mWh(1.0, 1.0), 1e-6);
    EXPECT_NEAR(integrator.getGeneration_mWh(), expected_mWh(1.0, 2.0), 1e-6);

    integrator.resetGeneration();
    EXPECT_NEAR(integrator.getConsumption_mWh(), expected_mWh(1.0, 1.0), 1e-6);
    EXPECT_FLOAT_EQ(integrator.getGeneration_mWh(), 0.0f);
}

TEST(PowerIntegratorTest, LongDurationAccumulationKeepsPrecision) {
    PowerIntegrator integrator;
    const double consumedW = 1.26;
    const double generatedW = 0.71;
    const int ticks = 30 * 24 * 3600;  // 30 days at 1 Hz

    for (int i = 0; i <= ticks; i++) {
        integrator.update(static_cast<double>(i), consumedW, generatedW);
    }

    // Accumulating 1 Hz increments in single precision drifts by several percent over this span
    EXPECT_NEAR(integrator.getConsumption_mWh(), expected_mWh(consumedW, ticks), expected_mWh(consumedW, ticks) * 1e-6);
    EXPECT_NEAR(integrator.getGeneration_mWh(), expected_mWh(generatedW, ticks),
                expected_mWh(generatedW, ticks) * 1e-6);
}
