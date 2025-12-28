#include <gtest/gtest.h>

#include <cmath>

#include "FprimeZephyrReference/Components/DetumbleManager/Magnetorquer.hpp"

using Components::Magnetorquer;

class MagnetorquerTest : public ::testing::Test {
  protected:
    Magnetorquer m_torquer;

    void SetUp() override {
        // Default configuration for a rectangular coil
        m_torquer.m_shape = Magnetorquer::RECTANGULAR;
        m_torquer.m_width = 0.1;   // 10 cm
        m_torquer.m_length = 0.2;  // 20 cm
        m_torquer.m_turns = 100.0;
        m_torquer.m_voltage = 5.0;      // 5V
        m_torquer.m_resistance = 10.0;  // 10 Ohms -> Max current = 0.5 A
        m_torquer.m_direction_sign = Magnetorquer::POSITIVE;
    }
};

TEST_F(MagnetorquerTest, RectangularAreaCalculation) {
    // Area = 0.1 * 0.2 = 0.02 m^2
    // Max Current = 5.0 / 10.0 = 0.5 A
    // Target Dipole for Max Current = N * I * A = 100 * 0.5 * 0.02 = 1.0 A*m^2

    // Test 100% positive
    // Expected: 127
    EXPECT_EQ(m_torquer.dipoleMomentToCurrent(1.0), 127);

    // Test 50% positive
    // Target Dipole = 0.5 A*m^2 -> Current = 0.25 A -> 50% of max -> ~64
    EXPECT_NEAR(m_torquer.dipoleMomentToCurrent(0.5), 64, 1);

    // Test 100% negative
    EXPECT_EQ(m_torquer.dipoleMomentToCurrent(-1.0), -127);
}

TEST_F(MagnetorquerTest, CircularAreaCalculation) {
    m_torquer.m_shape = Magnetorquer::CIRCULAR;
    m_torquer.m_diameter = 0.2;  // Radius = 0.1
    // Area = pi * 0.1^2 = 0.0314159... m^2
    // Max Current = 0.5 A
    // Max Dipole = 100 * 0.5 * 0.0314159 = 1.570795

    EXPECT_EQ(m_torquer.dipoleMomentToCurrent(1.570795), 127);
    EXPECT_NEAR(m_torquer.dipoleMomentToCurrent(1.570795 / 2.0), 64, 1);
}

TEST_F(MagnetorquerTest, Clamping) {
    // Max Dipole is 1.0 (from Rectangular setup)
    // Request 2.0
    EXPECT_EQ(m_torquer.dipoleMomentToCurrent(2.0), 127);
    EXPECT_EQ(m_torquer.dipoleMomentToCurrent(-2.0), -127);
}

TEST_F(MagnetorquerTest, DirectionSign) {
    m_torquer.m_direction_sign = Magnetorquer::NEGATIVE;
    // Request positive dipole -> should result in negative drive value because of sign flip
    // Max Dipole 1.0 -> Current 0.5A -> Scaled 127 -> * -1 -> -127
    EXPECT_EQ(m_torquer.dipoleMomentToCurrent(1.0), -127);
}

TEST_F(MagnetorquerTest, ZeroResistance) {
    m_torquer.m_resistance = 0.0;
    // Max current calculation should handle div by zero (return 0.0)
    EXPECT_EQ(m_torquer.dipoleMomentToCurrent(1.0), 0);
}

TEST_F(MagnetorquerTest, ZeroTurns) {
    m_torquer.m_turns = 0.0;
    // computeTargetCurrent: if turns == 0, return 0.0.
    EXPECT_EQ(m_torquer.dipoleMomentToCurrent(1.0), 0);
}

TEST_F(MagnetorquerTest, ZeroArea) {
    m_torquer.m_width = 0.0;
    // computeTargetCurrent: if area == 0, return 0.0.
    EXPECT_EQ(m_torquer.dipoleMomentToCurrent(1.0), 0);
}
