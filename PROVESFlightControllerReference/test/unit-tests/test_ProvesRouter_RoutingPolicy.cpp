#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/ProvesRouter/RoutingPolicy.hpp"

using Components::RoutingPolicy::resetsCommandLoss;

TEST(RoutingPolicyTest, AuthenticatedRoutedPacketResetsCommandLoss) {
    EXPECT_TRUE(resetsCommandLoss(true, true));
}

TEST(RoutingPolicyTest, BypassedPacketDoesNotResetCommandLoss) {
    // An unauthenticated NO_OP routed through the bypass must not hold off the dead-man
    EXPECT_FALSE(resetsCommandLoss(false, true));
}

TEST(RoutingPolicyTest, UnroutedPacketDoesNotResetCommandLoss) {
    EXPECT_FALSE(resetsCommandLoss(true, false));
    EXPECT_FALSE(resetsCommandLoss(false, false));
}
