#include <gtest/gtest.h>

#include <string>

#include "PROVESFlightControllerReference/Components/AntennaDeployer/DeploymentSequencer.hpp"

using Components::DeploymentSequencer;
using Kind = DeploymentSequencer::EffectKind;
using Result = DeploymentSequencer::Result;

namespace {

// Effects of a step as text, in order, e.g. "ATTEMPT(1,0) START"
std::string describe(const DeploymentSequencer::Step& step) {
    std::string text;
    for (std::size_t i = 0; i < step.size(); i++) {
        const DeploymentSequencer::Effect& effect = step[i];
        if (i > 0) {
            text += " ";
        }
        switch (effect.kind) {
            case Kind::BURN_START:
                text += "START";
                break;
            case Kind::BURN_STOP:
                text += "STOP";
                break;
            case Kind::ATTEMPT_STARTED:
                text += "ATTEMPT(" + std::to_string(effect.attempt) + "," + std::to_string(effect.attemptCount) + ")";
                break;
            case Kind::BURN_SIGNAL_COUNT:
                text += "COUNT(" + std::to_string(effect.burnTicks) + ")";
                break;
            case Kind::MARK_DEPLOYED:
                text += "DEPLOYED(" + std::to_string(effect.attempt) + ")";
                break;
            case Kind::FINISHED: {
                const char* names[] = {"SUCCESS", "ABORT", "FAILED"};
                text += std::string("FINISHED(") + names[static_cast<int>(effect.result)] + "," +
                        std::to_string(effect.attempt) + ")";
                break;
            }
        }
    }
    return text;
}

// 3 attempts, 2 tick burn, 3 tick wait
const DeploymentSequencer::Params PARAMS = {2, 3, 3};

std::string tick(DeploymentSequencer& sequencer, const DeploymentSequencer::Params& params = PARAMS) {
    return describe(sequencer.tick(params));
}

}  // namespace

// ----------------------------------------------------------------------
// Starting
// ----------------------------------------------------------------------

TEST(DeploymentSequencerStartTest, StartsFirstAttempt) {
    DeploymentSequencer sequencer;

    EXPECT_FALSE(sequencer.isBusy());
    EXPECT_EQ(describe(sequencer.start()), "ATTEMPT(1,0) START");
    EXPECT_TRUE(sequencer.isBusy());
}

TEST(DeploymentSequencerStartTest, StartWhileBusyDoesNothing) {
    DeploymentSequencer sequencer;
    sequencer.start();

    EXPECT_EQ(describe(sequencer.start()), "");
}

TEST(DeploymentSequencerStartTest, IdleTickDoesNothing) {
    DeploymentSequencer sequencer;

    EXPECT_EQ(tick(sequencer), "");
    EXPECT_FALSE(sequencer.isBusy());
}

// ----------------------------------------------------------------------
// Burn and retry
// ----------------------------------------------------------------------

TEST(DeploymentSequencerSequenceTest, BurnsForTheDurationThenWaits) {
    DeploymentSequencer sequencer;
    sequencer.start();

    EXPECT_EQ(tick(sequencer), "");
    EXPECT_EQ(tick(sequencer), "STOP COUNT(2)");
    EXPECT_TRUE(sequencer.isBusy());
}

TEST(DeploymentSequencerSequenceTest, WaitsTheRetryDelayThenBurnsAgain) {
    DeploymentSequencer sequencer;
    sequencer.start();
    tick(sequencer);
    tick(sequencer);

    EXPECT_EQ(tick(sequencer), "");
    EXPECT_EQ(tick(sequencer), "");
    EXPECT_EQ(tick(sequencer), "ATTEMPT(2,1) START");
}

TEST(DeploymentSequencerSequenceTest, GivesUpAfterMaxAttemptsAndMarksDeployed) {
    DeploymentSequencer sequencer;
    sequencer.start();

    // Attempt 1 and 2: burn 2 ticks, wait 3 ticks
    for (int attempt = 1; attempt <= 2; attempt++) {
        EXPECT_EQ(tick(sequencer), "");
        EXPECT_EQ(tick(sequencer), "STOP COUNT(2)");
        EXPECT_EQ(tick(sequencer), "");
        EXPECT_EQ(tick(sequencer), "");
        EXPECT_EQ(tick(sequencer),
                  "ATTEMPT(" + std::to_string(attempt + 1) + "," + std::to_string(attempt) + ") START");
    }

    // Attempt 3 is the last: FAILED is recorded as deployed, so the antenna is not burned again.
    // The burn is stopped twice: once at the end of the burn, once while finishing.
    EXPECT_EQ(tick(sequencer), "");
    EXPECT_EQ(tick(sequencer), "STOP COUNT(2) STOP DEPLOYED(3) FINISHED(FAILED,3)");
    EXPECT_FALSE(sequencer.isBusy());
}

TEST(DeploymentSequencerSequenceTest, ReportsEveryBurnSignalCount) {
    DeploymentSequencer sequencer;
    const DeploymentSequencer::Params longer = {4, 1, 2};
    sequencer.start();

    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(tick(sequencer, longer), "");
    }
    EXPECT_EQ(tick(sequencer, longer), "STOP COUNT(4)");
}

// ----------------------------------------------------------------------
// Edge parameters
// ----------------------------------------------------------------------

TEST(DeploymentSequencerParamsTest, ZeroRetryDelayRetriesOnTheNextTick) {
    DeploymentSequencer sequencer;
    const DeploymentSequencer::Params noDelay = {1, 0, 3};
    sequencer.start();

    EXPECT_EQ(tick(sequencer, noDelay), "STOP COUNT(1)");
    EXPECT_EQ(tick(sequencer, noDelay), "ATTEMPT(2,1) START");
}

TEST(DeploymentSequencerParamsTest, ZeroRetryDelayStillGivesUpAfterMaxAttempts) {
    DeploymentSequencer sequencer;
    const DeploymentSequencer::Params noDelay = {1, 0, 2};
    sequencer.start();

    tick(sequencer, noDelay);
    tick(sequencer, noDelay);
    EXPECT_EQ(tick(sequencer, noDelay), "STOP COUNT(1) STOP DEPLOYED(2) FINISHED(FAILED,2)");
}

TEST(DeploymentSequencerParamsTest, SingleAttemptFailsAfterFirstBurn) {
    DeploymentSequencer sequencer;
    const DeploymentSequencer::Params once = {1, 5, 1};
    sequencer.start();

    EXPECT_EQ(tick(sequencer, once), "STOP COUNT(1) STOP DEPLOYED(1) FINISHED(FAILED,1)");
    EXPECT_FALSE(sequencer.isBusy());
}

TEST(DeploymentSequencerParamsTest, ZeroMaxAttemptsStillBurnsOnceThenFails) {
    DeploymentSequencer sequencer;
    const DeploymentSequencer::Params none = {1, 5, 0};
    sequencer.start();

    EXPECT_EQ(tick(sequencer, none), "STOP COUNT(1) STOP DEPLOYED(1) FINISHED(FAILED,1)");
}

TEST(DeploymentSequencerParamsTest, ZeroBurnDurationStopsOnTheFirstTick) {
    DeploymentSequencer sequencer;
    const DeploymentSequencer::Params instant = {0, 2, 3};
    sequencer.start();

    EXPECT_EQ(tick(sequencer, instant), "STOP COUNT(1)");
}

TEST(DeploymentSequencerParamsTest, ParamsAreReadOnEveryTick) {
    DeploymentSequencer sequencer;
    sequencer.start();

    // Lengthening the burn mid-flight is honored
    const DeploymentSequencer::Params longer = {5, 3, 3};
    EXPECT_EQ(tick(sequencer, PARAMS), "");
    EXPECT_EQ(tick(sequencer, longer), "");
    EXPECT_EQ(tick(sequencer, longer), "");
}

// ----------------------------------------------------------------------
// Stopping
// ----------------------------------------------------------------------

TEST(DeploymentSequencerStopTest, StopWhileIdleDoesNothing) {
    DeploymentSequencer sequencer;

    EXPECT_EQ(describe(sequencer.stop()), "");
}

TEST(DeploymentSequencerStopTest, StopMidBurnAbortsWithoutMarkingDeployed) {
    DeploymentSequencer sequencer;
    sequencer.start();
    tick(sequencer);

    EXPECT_EQ(describe(sequencer.stop()), "STOP COUNT(1) FINISHED(ABORT,1)");
    EXPECT_FALSE(sequencer.isBusy());
}

TEST(DeploymentSequencerStopTest, StopBeforeTheFirstTickHasNoBurnCount) {
    DeploymentSequencer sequencer;
    sequencer.start();

    EXPECT_EQ(describe(sequencer.stop()), "STOP FINISHED(ABORT,1)");
}

TEST(DeploymentSequencerStopTest, StopMidWaitAbortsWithoutBurnCount) {
    DeploymentSequencer sequencer;
    sequencer.start();
    tick(sequencer);
    tick(sequencer);
    tick(sequencer);

    // The count was already reported when the burn ended
    EXPECT_EQ(describe(sequencer.stop()), "STOP FINISHED(ABORT,1)");
    EXPECT_FALSE(sequencer.isBusy());
}

TEST(DeploymentSequencerStopTest, StopReportsTheAttemptInProgress) {
    DeploymentSequencer sequencer;
    sequencer.start();
    // Burn 2 ticks, wait 3 ticks: the fifth tick starts attempt 2
    for (int i = 0; i < 5; i++) {
        tick(sequencer);
    }

    EXPECT_EQ(describe(sequencer.stop()), "STOP FINISHED(ABORT,2)");
}

TEST(DeploymentSequencerStopTest, StoppedSequencerIsIdleAndStartsOver) {
    DeploymentSequencer sequencer;
    sequencer.start();
    tick(sequencer);
    sequencer.stop();

    EXPECT_EQ(tick(sequencer), "");
    // The attempt number restarts at 1, the attempt count for telemetry keeps counting
    EXPECT_EQ(describe(sequencer.start()), "ATTEMPT(1,1) START");
}

TEST(DeploymentSequencerStopTest, SecondStopDoesNothing) {
    DeploymentSequencer sequencer;
    sequencer.start();
    sequencer.stop();

    EXPECT_EQ(describe(sequencer.stop()), "");
}

// ----------------------------------------------------------------------
// After finishing
// ----------------------------------------------------------------------

TEST(DeploymentSequencerRestartTest, CanDeployAgainAfterFailing) {
    DeploymentSequencer sequencer;
    const DeploymentSequencer::Params once = {1, 1, 1};
    sequencer.start();
    tick(sequencer, once);

    EXPECT_FALSE(sequencer.isBusy());
    EXPECT_EQ(describe(sequencer.start()), "ATTEMPT(1,1) START");
}

TEST(DeploymentSequencerStepTest, FullStepDropsFurtherEffects) {
    DeploymentSequencer::Step step;
    const DeploymentSequencer::Effect effect = {Kind::BURN_STOP, 0, 0, 0, Result::SUCCESS};

    // Copied, because EXPECT_EQ takes a reference and a constexpr member is not defined before C++17
    const std::size_t max = DeploymentSequencer::Step::MAX_EFFECTS;

    for (std::size_t i = 0; i < max; i++) {
        EXPECT_TRUE(step.add(effect));
    }
    EXPECT_FALSE(step.add(effect));
    EXPECT_EQ(step.size(), max);
}
