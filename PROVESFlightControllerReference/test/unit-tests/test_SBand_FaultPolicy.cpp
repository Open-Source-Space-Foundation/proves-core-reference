#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/SBand/SBandFaultPolicy.hpp"

using Components::SBandFaultPolicy;

TEST(SBandFaultPolicyTest, FreshPolicyReportsNoneAndStaysNoneBelowThreshold) {
    SBandFaultPolicy policy;

    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::NONE);

    // One less than the consecutive-failure limit: still NONE.
    for (std::uint32_t i = 0; i < SBandFaultPolicy::CONSECUTIVE_FAILURE_LIMIT - 1; i++) {
        policy.operationFailed();
    }
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::NONE);
}

TEST(SBandFaultPolicyTest, ConsecutiveFailureLimitRequestsResetExactlyOnce) {
    SBandFaultPolicy policy;

    for (std::uint32_t i = 0; i < SBandFaultPolicy::CONSECUTIVE_FAILURE_LIMIT; i++) {
        policy.operationFailed();
    }
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::REQUEST_RESET);

    // Further failures while the reset is pending must not re-trigger or
    // otherwise change the decision -- the reset is requested exactly once
    // until resetCompleted() is observed.
    policy.operationFailed();
    policy.operationFailed();
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::REQUEST_RESET);
}

TEST(SBandFaultPolicyTest, SuccessResetsConsecutiveFailureCount) {
    SBandFaultPolicy policy;

    for (std::uint32_t i = 0; i < SBandFaultPolicy::CONSECUTIVE_FAILURE_LIMIT - 1; i++) {
        policy.operationFailed();
    }
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::NONE);

    // A success anywhere clears the count; it now takes a fresh run of
    // CONSECUTIVE_FAILURE_LIMIT failures to request a reset.
    policy.operationSucceeded();

    for (std::uint32_t i = 0; i < SBandFaultPolicy::CONSECUTIVE_FAILURE_LIMIT - 1; i++) {
        policy.operationFailed();
    }
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::NONE);

    policy.operationFailed();
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::REQUEST_RESET);
}

//! Drive the policy through one full failure/reset escalation cycle:
//! CONSECUTIVE_FAILURE_LIMIT failures (REQUEST_RESET), then resetCompleted()
//! with no intervening success.
void driveOneResetCycle(SBandFaultPolicy& policy) {
    for (std::uint32_t i = 0; i < SBandFaultPolicy::CONSECUTIVE_FAILURE_LIMIT; i++) {
        policy.operationFailed();
    }
    policy.resetCompleted();
}

TEST(SBandFaultPolicyTest, ResetsWithoutInterveningSuccessLatchFaulted) {
    SBandFaultPolicy policy;

    for (std::uint32_t i = 0; i < SBandFaultPolicy::RESET_ATTEMPT_LIMIT - 1; i++) {
        driveOneResetCycle(policy);
        EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::NONE);
    }

    // The Mth reset without an intervening success latches FAULTED.
    driveOneResetCycle(policy);
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::FAULTED);

    // Latched: further failures and successes do not change the state.
    policy.operationFailed();
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::FAULTED);
    policy.operationSucceeded();
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::FAULTED);
    policy.resetCompleted();
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::FAULTED);
}

TEST(SBandFaultPolicyTest, GroundResetReArmsFromFaultedAndCanReLatch) {
    SBandFaultPolicy policy;

    // Drive to FAULTED.
    for (std::uint32_t i = 0; i < SBandFaultPolicy::RESET_ATTEMPT_LIMIT; i++) {
        driveOneResetCycle(policy);
    }
    ASSERT_EQ(policy.decision(), SBandFaultPolicy::Decision::FAULTED);

    // Ground commands a reset: policy re-arms to a clean state.
    policy.groundResetRequested();
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::NONE);

    // Re-init fails N more times: escalates to REQUEST_RESET again, exactly
    // as if starting fresh.
    for (std::uint32_t i = 0; i < SBandFaultPolicy::CONSECUTIVE_FAILURE_LIMIT; i++) {
        policy.operationFailed();
    }
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::REQUEST_RESET);

    // M more failed resets re-latch FAULTED.
    policy.resetCompleted();
    for (std::uint32_t i = 0; i < SBandFaultPolicy::RESET_ATTEMPT_LIMIT - 1; i++) {
        driveOneResetCycle(policy);
    }
    EXPECT_EQ(policy.decision(), SBandFaultPolicy::Decision::FAULTED);
}

TEST(SBandFaultPolicyTest, CountersReflectFailuresAndResetsForTelemetry) {
    SBandFaultPolicy policy;

    EXPECT_EQ(policy.consecutiveFailures(), 0u);
    EXPECT_EQ(policy.resetsSinceSuccess(), 0u);

    policy.operationFailed();
    policy.operationFailed();
    EXPECT_EQ(policy.consecutiveFailures(), 2u);

    policy.operationSucceeded();
    EXPECT_EQ(policy.consecutiveFailures(), 0u);

    driveOneResetCycle(policy);
    EXPECT_EQ(policy.resetsSinceSuccess(), 1u);
    EXPECT_EQ(policy.consecutiveFailures(), 0u);
}
