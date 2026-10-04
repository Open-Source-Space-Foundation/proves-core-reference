#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Persistence.hpp"

using namespace Components::SequencePersistence;

TEST(SequencePersistenceTest, IdleTickDoesNotWrite) {
    EXPECT_FALSE(needsWrite(OnDisk{true, 42u}, 42u));
}

TEST(SequencePersistenceTest, ChangedCounterWrites) {
    EXPECT_TRUE(needsWrite(OnDisk{true, 42u}, 43u));
    EXPECT_TRUE(needsWrite(OnDisk{true, 42u}, 0u));  // SET_SEQ_NUM may move the counter backwards
}

TEST(SequencePersistenceTest, SuccessfulWriteStopsFurtherWrites) {
    const OnDisk onDisk = afterWrite(43u, true);
    EXPECT_TRUE(onDisk.known);
    EXPECT_EQ(onDisk.value, 43u);
    EXPECT_FALSE(needsWrite(onDisk, 43u));
}

TEST(SequencePersistenceTest, FailedWriteRetriesNextTick) {
    OnDisk onDisk = afterWrite(43u, false);
    EXPECT_FALSE(onDisk.known);
    EXPECT_TRUE(needsWrite(onDisk, 43u));
    onDisk = afterWrite(43u, true);
    EXPECT_FALSE(needsWrite(onDisk, 43u));
}

TEST(SequencePersistenceTest, FailedSetSeqNumWriteIsRetriedEvenThoughCounterIsUnchanged) {
    // SET_SEQ_NUM fails after the open has truncated the file; the in-memory counter stays at the
    // old value, which the file no longer holds, so the next tick must rewrite it.
    const uint32_t counter = 42u;
    const OnDisk onDisk = afterWrite(7u, false);
    EXPECT_TRUE(needsWrite(onDisk, counter));
    EXPECT_FALSE(needsWrite(afterWrite(counter, true), counter));
}

TEST(SequencePersistenceTest, PrepareForRebootFlushUsesSamePolicy) {
    // A reset frame advances the counter and reboots the board before the next tick: the
    // prepareForReboot flush must write exactly that value, and a tick that follows the flush
    // (or a flush that follows a tick) has nothing left to write.
    const OnDisk onDisk{true, 42u};
    const uint32_t resetFrame = 43u;
    EXPECT_TRUE(needsWrite(onDisk, resetFrame));
    const OnDisk afterFlush = afterWrite(resetFrame, true);
    EXPECT_EQ(afterFlush.value, resetFrame);
    EXPECT_FALSE(needsWrite(afterFlush, resetFrame));
}

TEST(SequencePersistenceTest, CounterAdvancingDuringFailedWriteIsPersistedNextTick) {
    // Frames keep being accepted while a write fails; the retry writes the newest value, not the old one.
    const OnDisk onDisk = afterWrite(43u, false);
    EXPECT_TRUE(needsWrite(onDisk, 50u));
    EXPECT_EQ(afterWrite(50u, true).value, 50u);
}
