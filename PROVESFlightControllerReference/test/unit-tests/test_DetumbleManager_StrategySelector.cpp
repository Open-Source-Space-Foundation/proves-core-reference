#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/DetumbleManager/StrategySelector.hpp"

using Components::StrategySelector;

TEST(StrategySelectorTest, IdleBelowLowerThreshold) {
    StrategySelector selector;
    // Configure: Max=10, Upper=5, Lower=1
    selector.configure(10.0, 5.0, 1.0);

    // Input: 0.5 deg/s (Below Lower 1.0)
    auto result = selector.fromAngularVelocityMagnitude(0.5);

    EXPECT_EQ(result, StrategySelector::IDLE);
}

TEST(StrategySelectorTest, HysteresisAboveMaxThreshold) {
    StrategySelector selector;
    selector.configure(10.0, 5.0, 1.0);

    // Input: 15.0 deg/s (Above Max 10.0)
    auto result = selector.fromAngularVelocityMagnitude(15.0);

    EXPECT_EQ(result, StrategySelector::HYSTERESIS);
}

TEST(StrategySelectorTest, BdotInActiveRange) {
    StrategySelector selector;
    selector.configure(10.0, 5.0, 1.0);

    // Input: 8.0 deg/s (Between Lower 1.0 and Max 10.0)
    auto result = selector.fromAngularVelocityMagnitude(8.0);

    EXPECT_EQ(result, StrategySelector::BDOT);
}

TEST(StrategySelectorTest, DeadbandCycleBehavior) {
    StrategySelector selector;
    // Configure: Max=10, Upper=5, Lower=1
    selector.configure(10.0, 5.0, 1.0);

    // 1. Start very low to reset state to "Holding/Idle"
    // Input < Lower(1.0) -> Target becomes Upper(5.0)
    auto res1 = selector.fromAngularVelocityMagnitude(0.5);
    EXPECT_EQ(res1, StrategySelector::IDLE);

    // 2. Increase velocity (Spin up), but stay below Upper(5.0)
    // 1.0 < 3.0 < 5.0. Target is 5.0.
    // Should stay IDLE because we haven't broken out of the deadband yet
    auto res2 = selector.fromAngularVelocityMagnitude(3.0);
    EXPECT_EQ(res2, StrategySelector::IDLE);

    // 3. Exceed Upper(5.0)
    // 6.0 >= 5.0. Target becomes Lower(1.0).
    // Should switch to BDOT
    auto res3 = selector.fromAngularVelocityMagnitude(6.0);
    EXPECT_EQ(res3, StrategySelector::BDOT);

    // 4. Decrease velocity (Detumble), but stay above Lower(1.0)
    // 1.0 < 3.0 < 5.0. Target is 1.0.
    // Should stay BDOT because we are "detumbling down" and haven't reached the floor
    auto res4 = selector.fromAngularVelocityMagnitude(3.0);
    EXPECT_EQ(res4, StrategySelector::BDOT);

    // 5. Drop below Lower(1.0)
    // 0.5 < 1.0. Target becomes Upper(5.0).
    // Should switch to IDLE
    auto res5 = selector.fromAngularVelocityMagnitude(0.5);
    EXPECT_EQ(res5, StrategySelector::IDLE);
}

TEST(StrategySelectorTest, BoundaryConditions) {
    StrategySelector selector;
    selector.configure(10.0, 5.0, 1.0);

    // Exact Max Threshold -> HYSTERESIS
    EXPECT_EQ(selector.fromAngularVelocityMagnitude(10.0), StrategySelector::HYSTERESIS);

    // Exact Lower Threshold (when target is lower) -> BDOT
    // 1.0 >= 1.0
    EXPECT_EQ(selector.fromAngularVelocityMagnitude(1.0), StrategySelector::BDOT);

    // Force target to Upper by going low
    selector.fromAngularVelocityMagnitude(0.0);

    // Exact Upper Threshold (when target is upper) -> BDOT
    // 5.0 >= 5.0
    EXPECT_EQ(selector.fromAngularVelocityMagnitude(5.0), StrategySelector::BDOT);
}
