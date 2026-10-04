#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/TcSecurityDeframer/Persistence.hpp"

using namespace Components::SequencePersistence;

TEST(SequencePersistenceTest, IdleTickDoesNotWrite) {
    EXPECT_FALSE(needsWrite(42u, 42u));
}

TEST(SequencePersistenceTest, ChangedCounterWrites) {
    EXPECT_TRUE(needsWrite(42u, 43u));
    EXPECT_TRUE(needsWrite(42u, 0u));  // SET_SEQ_NUM may move the counter backwards
}

TEST(SequencePersistenceTest, SuccessfulWriteStopsFurtherWrites) {
    const uint32_t persisted = afterWrite(42u, 43u, true);
    EXPECT_EQ(persisted, 43u);
    EXPECT_FALSE(needsWrite(persisted, 43u));
}

TEST(SequencePersistenceTest, FailedWriteRetriesNextTick) {
    uint32_t persisted = afterWrite(42u, 43u, false);
    EXPECT_EQ(persisted, 42u);
    EXPECT_TRUE(needsWrite(persisted, 43u));
    persisted = afterWrite(persisted, 43u, true);
    EXPECT_FALSE(needsWrite(persisted, 43u));
}

TEST(SequencePersistenceTest, PrepareForRebootFlushUsesSamePolicy) {
    // A reset frame advances the counter and reboots the board before the next tick: the
    // prepareForReboot flush must write exactly that value, and a tick that follows the flush
    // (or a flush that follows a tick) has nothing left to write.
    const uint32_t onDisk = 42u;
    const uint32_t resetFrame = 43u;
    EXPECT_TRUE(needsWrite(onDisk, resetFrame));
    const uint32_t afterFlush = afterWrite(onDisk, resetFrame, true);
    EXPECT_EQ(afterFlush, resetFrame);
    EXPECT_FALSE(needsWrite(afterFlush, resetFrame));
}

TEST(SequencePersistenceTest, CounterAdvancingDuringFailedWriteIsPersistedNextTick) {
    // Frames keep being accepted while a write fails; the retry writes the newest value, not the old one.
    const uint32_t persisted = afterWrite(42u, 43u, false);
    EXPECT_TRUE(needsWrite(persisted, 50u));
    EXPECT_EQ(afterWrite(persisted, 50u, true), 50u);
}
