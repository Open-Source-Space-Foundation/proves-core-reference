// ======================================================================
// \title  test_ComDelay_ComDelayLogic.cpp
// \brief  Host unit tests for Components::ComDelayLogic, the extracted
//         tick/divider/latch state machine used by the ComDelay F' component.
//
// ComDelay gates radio comStatus by a DIVIDER parameter: a status latched via
// comStatusIn is only released on a `run` tick when the internal counter is
// at 0, then the counter is re-armed against DIVIDER (or the default divider
// if the parameter is currently invalid/uninitialized).
// ======================================================================

#include <gtest/gtest.h>

#include "PROVESFlightControllerReference/Components/ComDelay/ComDelayLogic.hpp"

using Components::ComDelayLogic;
using Components::COM_DELAY_DEFAULT_DIVIDER;

namespace {

//! Drive `count` ticks with a fixed divider, returning the number of ticks
//! that produced an emission.
int countEmissions(ComDelayLogic& logic, std::uint16_t divider, bool dividerValid, int count) {
    int emissions = 0;
    bool status = false;
    for (int i = 0; i < count; ++i) {
        if (logic.tick(divider, dividerValid, status)) {
            ++emissions;
        }
    }
    return emissions;
}

}  // namespace

// ----------------------------------------------------------------------
// (1) Tick-paced release: latched status is only released on a tick where
//     the internal counter is at 0; it is not released just because it was
//     latched.
// ----------------------------------------------------------------------

TEST(ComDelayLogicTest, LatchedStatusNotReleasedUntilCounterIsZero) {
    ComDelayLogic logic;
    bool status = false;

    // First call: counter starts at 0 (a release slot), but nothing is
    // latched yet, so no emission -- and the counter advances past 0.
    EXPECT_FALSE(logic.tick(/*divider=*/3, /*dividerValid=*/true, status));  // tick_count 0 -> 1

    // Now latch a status while mid-cycle (counter != 0).
    logic.latchStatus(true);
    EXPECT_TRUE(logic.hasLatchedStatus());

    // Ticks while counter is nonzero must not release it.
    EXPECT_FALSE(logic.tick(3, true, status));  // tick_count 1 -> 2
    EXPECT_FALSE(logic.tick(3, true, status));  // tick_count 2 -> 3
    EXPECT_TRUE(logic.hasLatchedStatus());       // still latched, not lost

    // tick_count was 3 (>= divider 3) so it resets to 0 on this call, but the
    // *release check* for this call examines the counter as it was going in
    // (3, i.e. not 0), so still no emission this call...
    EXPECT_FALSE(logic.tick(3, true, status));  // tick_count 3 -> 0 (reset)

    // ...and the release happens on the *next* tick, where the counter is 0.
    ASSERT_TRUE(logic.tick(3, true, status));
    EXPECT_TRUE(status);
    EXPECT_FALSE(logic.hasLatchedStatus());
}

// ----------------------------------------------------------------------
// (2) Divider math: DIVIDER=N releases every N+1 ticks for N well below the
//     8-bit tick-counter width. The default divider (299) is a special case:
//     the production tick counter is a U8 (max 255), so DIVIDER=299 can never
//     be reached by comparison -- the counter instead wraps via 8-bit integer
//     overflow at 256, releasing a latched status every 256 ticks, not every
//     300. ComDelayLogic intentionally mirrors this pre-existing production
//     behavior (see ComDelay.cpp); this test documents/pins it rather than
//     asserting the aspirational "30s" comment in ComDelay.fpp.
// ----------------------------------------------------------------------

TEST(ComDelayLogicTest, SmallDividerReleasesEveryNPlus1Ticks) {
    for (std::uint16_t divider : {0, 1, 2, 3, 5, 10}) {
        ComDelayLogic logic;
        logic.latchStatus(true);

        const int period = divider + 1;
        // Exactly one emission every `period` ticks, across several cycles.
        for (int cycle = 0; cycle < 3; ++cycle) {
            int emissions = countEmissions(logic, divider, true, period);
            EXPECT_EQ(emissions, 1) << "divider=" << divider << " cycle=" << cycle;
            // Re-latch for the next cycle so we can keep observing releases.
            logic.latchStatus(true);
        }
    }
}

TEST(ComDelayLogicTest, DefaultDividerActuallyWrapsAt256TicksNot300) {
    ComDelayLogic logic;
    ASSERT_EQ(COM_DELAY_DEFAULT_DIVIDER, 299);

    logic.latchStatus(true);
    bool status = false;

    // 255 ticks: counter goes 0(consume immediately since starts at 0)..254->255,
    // no further emission expected inside this stretch beyond the first.
    // Re-latch and measure the *next* full period explicitly.
    ASSERT_TRUE(logic.tick(COM_DELAY_DEFAULT_DIVIDER, true, status));  // consumes immediately (counter starts at 0)
    logic.latchStatus(true);

    int emissions_in_255 = countEmissions(logic, COM_DELAY_DEFAULT_DIVIDER, true, 255);
    EXPECT_EQ(emissions_in_255, 0) << "should not release before the U8 counter wraps";

    // The 256th tick is where the U8 counter has wrapped back to 0.
    ASSERT_TRUE(logic.tick(COM_DELAY_DEFAULT_DIVIDER, true, status));
    EXPECT_TRUE(status);
}

// ----------------------------------------------------------------------
// (3) Status consumed exactly once: no double-emit of the same latched value.
// ----------------------------------------------------------------------

TEST(ComDelayLogicTest, StatusConsumedExactlyOnce) {
    ComDelayLogic logic;
    logic.latchStatus(true);
    bool status = false;

    ASSERT_TRUE(logic.tick(0, true, status));  // divider 0 -> period 1, immediate release slot
    EXPECT_TRUE(status);
    EXPECT_FALSE(logic.hasLatchedStatus());

    // Subsequent ticks at counter==0 (divider 0 means every tick is a release
    // slot) must not re-emit the already-consumed status.
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(logic.tick(0, true, status)) << "tick " << i << " must not double-emit";
    }
}

// ----------------------------------------------------------------------
// (4) No release when no status has been latched.
// ----------------------------------------------------------------------

TEST(ComDelayLogicTest, NoEmissionWhenNothingLatched) {
    ComDelayLogic logic;
    bool status = false;

    EXPECT_FALSE(logic.hasLatchedStatus());
    for (int i = 0; i < 10; ++i) {
        EXPECT_FALSE(logic.tick(0, true, status)) << "tick " << i;
    }
}

// ----------------------------------------------------------------------
// (5) Runtime divider change mid-cycle doesn't lose or double-emit an
//     already-latched status.
// ----------------------------------------------------------------------

TEST(ComDelayLogicTest, DividerChangeMidCycleDoesNotLoseOrDoubleEmitLatch) {
    ComDelayLogic logic;
    bool status = false;

    // Consume the free tick_count==0 slot first so we start a fresh cycle.
    ASSERT_FALSE(logic.tick(10, true, status));  // nothing latched yet; tick_count 0 -> 1
    logic.latchStatus(true);

    // Advance partway through a divider=10 cycle.
    EXPECT_FALSE(logic.tick(10, true, status));  // tick_count 1 -> 2
    EXPECT_FALSE(logic.tick(10, true, status));  // tick_count 2 -> 3

    // Now shrink the divider mid-cycle. The counter (3) is compared against
    // the *new* divider each call; shrinking to 3 means this call's condition
    // (tick_count 3 >= divider 3) is true, so the counter resets to 0 -- but,
    // per the state machine's rules, the release check for this same call
    // used the counter as it entered (3, not 0), so still no release yet.
    EXPECT_FALSE(logic.tick(3, true, status));  // tick_count 3 -> 0 (reset by new, smaller divider)
    EXPECT_TRUE(logic.hasLatchedStatus());        // still latched -- not lost

    // The very next tick sees counter==0 and releases exactly the one latched status.
    ASSERT_TRUE(logic.tick(3, true, status));
    EXPECT_TRUE(status);
    EXPECT_FALSE(logic.hasLatchedStatus());

    // And it must not double-emit afterward.
    EXPECT_FALSE(logic.tick(3, true, status));
}

TEST(ComDelayLogicTest, DividerGrowthMidCycleAlsoPreservesLatch) {
    ComDelayLogic logic;
    bool status = false;

    EXPECT_FALSE(logic.tick(2, true, status));  // tick_count 0 -> 1, nothing latched yet
    logic.latchStatus(true);

    // Grow the divider mid-cycle (e.g. 2 -> 50): the counter keeps counting
    // up toward the new, larger threshold without losing the latch. Counter
    // is currently 1; walk it up to 50 (49 more non-releasing calls).
    for (int i = 0; i < 49; ++i) {
        EXPECT_FALSE(logic.tick(50, true, status)) << "premature release at i=" << i;
    }
    EXPECT_TRUE(logic.hasLatchedStatus());

    // tick_count is now 50: this call's release check sees 50 (not 0), so no
    // emission yet, but the count-vs-divider comparison resets it to 0.
    EXPECT_FALSE(logic.tick(50, true, status));

    // The next call sees counter==0 and releases exactly the one latched status.
    ASSERT_TRUE(logic.tick(50, true, status));
    EXPECT_TRUE(status);
    EXPECT_FALSE(logic.hasLatchedStatus());
}

// ----------------------------------------------------------------------
// (7) Parameter-invalid fallback to the default divider.
// ----------------------------------------------------------------------

TEST(ComDelayLogicTest, InvalidDividerParamFallsBackToDefault) {
    ComDelayLogic logic;
    logic.latchStatus(true);
    bool status = false;

    // Consume the immediate 0-slot first.
    ASSERT_TRUE(logic.tick(/*divider=*/5, /*dividerValid=*/false, status));
    logic.latchStatus(true);

    // With dividerValid=false, the requested divider (5) must be ignored in
    // favor of COM_DELAY_DEFAULT_DIVIDER (299) -- i.e. it must NOT release
    // after only 5 more ticks.
    int emissions = countEmissions(logic, /*divider=*/5, /*dividerValid=*/false, 5);
    EXPECT_EQ(emissions, 0);
    EXPECT_TRUE(logic.hasLatchedStatus());
}

TEST(ComDelayLogicTest, ValidZeroDividerIsNotTreatedAsInvalid) {
    // Guards against a fallback implementation that mistakes DIVIDER==0 for
    // "invalid" and silently substitutes the default divider.
    ComDelayLogic logic;
    logic.latchStatus(true);
    bool status = false;

    ASSERT_TRUE(logic.tick(/*divider=*/0, /*dividerValid=*/true, status));
    EXPECT_TRUE(status);
}
