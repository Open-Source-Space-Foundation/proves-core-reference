// ======================================================================
// \title  test_FlashWorker_UpdateSequencer.cpp
// \brief  Unit tests for the flight software update sequencing helper
// ======================================================================

#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/FlashWorker/UpdateSequencer.hpp"

using Components::UpdateSequencer;

namespace {

//! Drive a sequencer to the PREPARED state
UpdateSequencer prepared() {
    UpdateSequencer sequencer;
    EXPECT_EQ(UpdateSequencer::Status::OP_OK, sequencer.onPrepareComplete(true));
    return sequencer;
}

}  // namespace

// ----------------------------------------------------------------------
// Initial state
// ----------------------------------------------------------------------

TEST(UpdateSequencerTest, StartsIdleAndUnprepared) {
    UpdateSequencer sequencer;
    EXPECT_EQ(UpdateSequencer::Step::IDLE, sequencer.step());
    EXPECT_FALSE(sequencer.isPrepared());
}

// ----------------------------------------------------------------------
// Preparation
// ----------------------------------------------------------------------

TEST(UpdateSequencerTest, SuccessfulPrepareBecomesPrepared) {
    UpdateSequencer sequencer;
    EXPECT_EQ(UpdateSequencer::Status::OP_OK, sequencer.onPrepareComplete(true));
    EXPECT_EQ(UpdateSequencer::Step::PREPARED, sequencer.step());
    EXPECT_TRUE(sequencer.isPrepared());
}

TEST(UpdateSequencerTest, FailedPrepareReportsPreparationErrorAndStaysIdle) {
    UpdateSequencer sequencer;
    EXPECT_EQ(UpdateSequencer::Status::PREPARATION_ERROR, sequencer.onPrepareComplete(false));
    EXPECT_EQ(UpdateSequencer::Step::IDLE, sequencer.step());
    EXPECT_FALSE(sequencer.isPrepared());
}

TEST(UpdateSequencerTest, FailedPrepareAfterSuccessfulPrepareDropsToIdle) {
    UpdateSequencer sequencer = prepared();
    EXPECT_EQ(UpdateSequencer::Status::PREPARATION_ERROR, sequencer.onPrepareComplete(false));
    EXPECT_EQ(UpdateSequencer::Step::IDLE, sequencer.step());
}

// ----------------------------------------------------------------------
// Ordering: an update requires a preparation
// ----------------------------------------------------------------------

TEST(UpdateSequencerTest, UpdateWithoutPrepareIsRejected) {
    UpdateSequencer sequencer;
    EXPECT_EQ(UpdateSequencer::Status::UNPREPARED, sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::SUCCESS));
    EXPECT_EQ(UpdateSequencer::Step::IDLE, sequencer.step());
}

TEST(UpdateSequencerTest, SecondUpdateWithoutRepreparingIsRejected) {
    UpdateSequencer sequencer = prepared();
    EXPECT_EQ(UpdateSequencer::Status::OP_OK, sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::SUCCESS));
    EXPECT_EQ(UpdateSequencer::Step::UPDATED, sequencer.step());
    // The slot now holds an image; writing another without erasing first must not be allowed
    EXPECT_EQ(UpdateSequencer::Status::UNPREPARED, sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::SUCCESS));
    EXPECT_EQ(UpdateSequencer::Step::UPDATED, sequencer.step());
}

TEST(UpdateSequencerTest, PrepareAfterUpdateAllowsAnotherUpdate) {
    UpdateSequencer sequencer = prepared();
    EXPECT_EQ(UpdateSequencer::Status::OP_OK, sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::SUCCESS));
    EXPECT_EQ(UpdateSequencer::Status::OP_OK, sequencer.onPrepareComplete(true));
    EXPECT_TRUE(sequencer.isPrepared());
    EXPECT_EQ(UpdateSequencer::Status::OP_OK, sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::SUCCESS));
}

// ----------------------------------------------------------------------
// Failures are reported as failures
//
// Regression coverage for the defect where every non-CRC failure returned OP_OK, causing the
// Updater component to emit UpdateSucceeded for an image that was never fully written.
// ----------------------------------------------------------------------

TEST(UpdateSequencerTest, FileOpenFailureReportsReadError) {
    UpdateSequencer sequencer = prepared();
    EXPECT_EQ(UpdateSequencer::Status::IMAGE_FILE_READ_ERROR,
              sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::FILE_OPEN_FAILED));
}

TEST(UpdateSequencerTest, FileQueryFailureReportsReadError) {
    UpdateSequencer sequencer = prepared();
    EXPECT_EQ(UpdateSequencer::Status::IMAGE_FILE_READ_ERROR,
              sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::FILE_QUERY_FAILED));
}

TEST(UpdateSequencerTest, FileReadFailureReportsReadError) {
    UpdateSequencer sequencer = prepared();
    EXPECT_EQ(UpdateSequencer::Status::IMAGE_FILE_READ_ERROR,
              sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::FILE_READ_FAILED));
}

TEST(UpdateSequencerTest, CrcMismatchReportsCrcMismatch) {
    UpdateSequencer sequencer = prepared();
    EXPECT_EQ(UpdateSequencer::Status::IMAGE_CRC_MISMATCH,
              sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::CRC_MISMATCH));
}

TEST(UpdateSequencerTest, FlashWriteFailureReportsFlashWriteError) {
    UpdateSequencer sequencer = prepared();
    EXPECT_EQ(UpdateSequencer::Status::FLASH_WRITE_ERROR,
              sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::FLASH_WRITE_FAILED));
}

TEST(UpdateSequencerTest, NoFailureOutcomeReportsSuccess) {
    // Every outcome other than SUCCESS must map to a non-OK status
    const UpdateSequencer::WriteOutcome failures[] = {
        UpdateSequencer::WriteOutcome::FILE_OPEN_FAILED,   UpdateSequencer::WriteOutcome::FILE_QUERY_FAILED,
        UpdateSequencer::WriteOutcome::CRC_MISMATCH,       UpdateSequencer::WriteOutcome::FILE_READ_FAILED,
        UpdateSequencer::WriteOutcome::FLASH_WRITE_FAILED,
    };
    for (const UpdateSequencer::WriteOutcome outcome : failures) {
        EXPECT_NE(UpdateSequencer::Status::OP_OK, UpdateSequencer::statusForOutcome(outcome));
    }
    EXPECT_EQ(UpdateSequencer::Status::OP_OK,
              UpdateSequencer::statusForOutcome(UpdateSequencer::WriteOutcome::SUCCESS));
}

// ----------------------------------------------------------------------
// Retry cost: only failures that reached the flash force another erase
// ----------------------------------------------------------------------

TEST(UpdateSequencerTest, FailuresBeforeAnyFlashWriteStayPrepared) {
    // A mistyped file name or a bad CRC never reaches the flash, so the erased slot is still good
    // and the operator can retry immediately instead of paying for another 1 MB erase on orbit.
    const UpdateSequencer::WriteOutcome clean[] = {
        UpdateSequencer::WriteOutcome::FILE_OPEN_FAILED,
        UpdateSequencer::WriteOutcome::FILE_QUERY_FAILED,
        UpdateSequencer::WriteOutcome::CRC_MISMATCH,
    };
    for (const UpdateSequencer::WriteOutcome outcome : clean) {
        UpdateSequencer sequencer = prepared();
        EXPECT_FALSE(UpdateSequencer::dirtiesStagingSlot(outcome));
        sequencer.onUpdateComplete(outcome);
        EXPECT_TRUE(sequencer.isPrepared()) << "outcome " << static_cast<int>(outcome) << " should be retryable";
    }
}

TEST(UpdateSequencerTest, FailuresPartWayThroughRequireAnotherPrepare) {
    // These leave a partial image behind, so the slot must be erased again before another attempt
    const UpdateSequencer::WriteOutcome dirty[] = {
        UpdateSequencer::WriteOutcome::FILE_READ_FAILED,
        UpdateSequencer::WriteOutcome::FLASH_WRITE_FAILED,
    };
    for (const UpdateSequencer::WriteOutcome outcome : dirty) {
        UpdateSequencer sequencer = prepared();
        EXPECT_TRUE(UpdateSequencer::dirtiesStagingSlot(outcome));
        sequencer.onUpdateComplete(outcome);
        EXPECT_EQ(UpdateSequencer::Step::IDLE, sequencer.step());
        EXPECT_FALSE(sequencer.isPrepared());
        // And the retry is correctly refused until a preparation runs
        EXPECT_EQ(UpdateSequencer::Status::UNPREPARED,
                  sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::SUCCESS));
    }
}

// ----------------------------------------------------------------------
// Stage reporting
// ----------------------------------------------------------------------

TEST(UpdateSequencerTest, SettledStageFollowsTheSequence) {
    UpdateSequencer sequencer;
    EXPECT_EQ(UpdateSequencer::Stage::IDLE, sequencer.settledStage());
    sequencer.onPrepareComplete(true);
    EXPECT_EQ(UpdateSequencer::Stage::PREPARED, sequencer.settledStage());
    sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::SUCCESS);
    EXPECT_EQ(UpdateSequencer::Stage::UPDATED, sequencer.settledStage());
}

TEST(UpdateSequencerTest, SettledStageReportsFailure) {
    UpdateSequencer sequencer;
    sequencer.onPrepareComplete(false);
    EXPECT_EQ(UpdateSequencer::Stage::FAILED, sequencer.settledStage());

    UpdateSequencer other = prepared();
    other.onUpdateComplete(UpdateSequencer::WriteOutcome::FLASH_WRITE_FAILED);
    EXPECT_EQ(UpdateSequencer::Stage::FAILED, other.settledStage());
}

TEST(UpdateSequencerTest, SettledStageClearsFailureAfterRecovery) {
    UpdateSequencer sequencer = prepared();
    sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::FLASH_WRITE_FAILED);
    EXPECT_EQ(UpdateSequencer::Stage::FAILED, sequencer.settledStage());
    // A successful re-preparation must not leave stale failure state in telemetry
    sequencer.onPrepareComplete(true);
    EXPECT_EQ(UpdateSequencer::Stage::PREPARED, sequencer.settledStage());
}

// ----------------------------------------------------------------------
// Progress reporting
// ----------------------------------------------------------------------

TEST(UpdateSequencerTest, PercentCompleteIsProportional) {
    EXPECT_EQ(0, UpdateSequencer::percentComplete(0, 1000));
    EXPECT_EQ(25, UpdateSequencer::percentComplete(250, 1000));
    EXPECT_EQ(50, UpdateSequencer::percentComplete(500, 1000));
    EXPECT_EQ(100, UpdateSequencer::percentComplete(1000, 1000));
}

TEST(UpdateSequencerTest, PercentCompleteHandlesDegenerateSizes) {
    // An unreadable size must not divide by zero
    EXPECT_EQ(0, UpdateSequencer::percentComplete(0, 0));
    EXPECT_EQ(0, UpdateSequencer::percentComplete(500, 0));
    // Nor may over-reporting produce a percentage above 100
    EXPECT_EQ(100, UpdateSequencer::percentComplete(2000, 1000));
}

TEST(UpdateSequencerTest, PercentCompleteFloorsAtRealisticImageSizes) {
    // Sizes taken from a real signed image (726784 bytes). Percentage floors rather than rounds, so
    // telemetry never claims 100% before the last byte is written.
    EXPECT_EQ(50, UpdateSequencer::percentComplete(363392, 726784));
    EXPECT_EQ(98, UpdateSequencer::percentComplete(719516, 726784));
    EXPECT_EQ(99, UpdateSequencer::percentComplete(726000, 726784));
    EXPECT_EQ(100, UpdateSequencer::percentComplete(726784, 726784));
}

TEST(UpdateSequencerTest, ProgressReportsEveryStep) {
    EXPECT_FALSE(UpdateSequencer::progressReportDue(0, 0, 10));
    EXPECT_FALSE(UpdateSequencer::progressReportDue(9, 0, 10));
    EXPECT_TRUE(UpdateSequencer::progressReportDue(10, 0, 10));
    EXPECT_FALSE(UpdateSequencer::progressReportDue(15, 10, 10));
    EXPECT_TRUE(UpdateSequencer::progressReportDue(20, 10, 10));
}

TEST(UpdateSequencerTest, ProgressAlwaysReportsCompletionExactlyOnce) {
    // Completion matters even when it does not land on a step boundary
    EXPECT_TRUE(UpdateSequencer::progressReportDue(100, 95, 10));
    EXPECT_FALSE(UpdateSequencer::progressReportDue(100, 100, 10));
}

TEST(UpdateSequencerTest, ProgressStepOfZeroDoesNotSilenceReporting) {
    // A misconfigured parameter must not disable progress entirely
    EXPECT_TRUE(UpdateSequencer::progressReportDue(1, 0, 0));
    EXPECT_TRUE(UpdateSequencer::progressReportDue(50, 49, 0));
}

TEST(UpdateSequencerTest, ProgressDoesNotReportGoingBackwards) {
    EXPECT_FALSE(UpdateSequencer::progressReportDue(10, 50, 10));
}

TEST(UpdateSequencerTest, RetryAfterCleanFailureCanSucceed) {
    UpdateSequencer sequencer = prepared();
    EXPECT_EQ(UpdateSequencer::Status::IMAGE_CRC_MISMATCH,
              sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::CRC_MISMATCH));
    EXPECT_EQ(UpdateSequencer::Status::OP_OK, sequencer.onUpdateComplete(UpdateSequencer::WriteOutcome::SUCCESS));
    EXPECT_EQ(UpdateSequencer::Step::UPDATED, sequencer.step());
}
