#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "PROVESFlightControllerReference/Components/ModeManager/CommandLossTimer.hpp"

using Components::CommandLossTimer;

namespace {

constexpr std::uint32_t kThreeDays = 3 * 24 * 60 * 60;
const std::uint64_t kPeriod = CommandLossTimer::PERSIST_PERIOD_S;
// Each boot credits one period, so a single boot trips one period before the limit.
const std::uint64_t kTrip = kThreeDays - kPeriod;

// ModeManager's glue around the timer: a file that survives reboots, read back
// through decode() on boot, written once on boot and then whenever tick() asks.
struct Board {
    std::vector<std::uint8_t> file;  // empty == file missing
    CommandLossTimer timer;
    std::uint32_t limit = kThreeDays;
    std::uint64_t uptime = 0;
    std::vector<std::uint64_t> writeTimes;
    std::vector<std::uint32_t> writeValues;
    std::vector<std::uint64_t> expiryTimes;

    void boot() {
        timer = CommandLossTimer();
        std::uint32_t stored = 0;
        if (!CommandLossTimer::decode(file.data(), file.size(), stored)) {
            stored = 0;
        }
        write(timer.resume(stored));
    }

    void write(std::uint32_t value) {
        std::uint8_t record[CommandLossTimer::RECORD_SIZE];
        CommandLossTimer::encode(value, record);
        file.assign(record, record + sizeof(record));
        writeTimes.push_back(uptime);
        writeValues.push_back(value);
    }

    void tick() {
        uptime++;
        CommandLossTimer::TickResult r = timer.tick(limit);
        if (r.persist) {
            write(r.persistValue);
        }
        if (r.expired) {
            expiryTimes.push_back(uptime);
        }
    }

    void run(std::uint64_t seconds) {
        for (std::uint64_t i = 0; i < seconds; i++) {
            tick();
        }
    }
};

}  // namespace

TEST(CommandLossTimerTest, TripsAtLimitWithoutReboot) {
    Board b;
    b.boot();
    b.run(kThreeDays);
    ASSERT_EQ(b.expiryTimes.size(), 1u);
    EXPECT_EQ(b.expiryTimes[0], kTrip);
}

TEST(CommandLossTimerTest, ResumeAfterRebootTripsAtTotalElapsedTime) {
    Board b;
    b.boot();
    b.run(36 * 60 * 60);  // 1.5 days of silence, reboot right after a persist
    ASSERT_FALSE(b.writeTimes.empty());
    ASSERT_EQ(b.writeTimes.back(), b.uptime);
    b.boot();
    b.run(kThreeDays);
    ASSERT_EQ(b.expiryTimes.size(), 1u);
    EXPECT_EQ(b.expiryTimes[0], kTrip - kPeriod);
}

TEST(CommandLossTimerTest, RebootMidPeriodErrsByAtMostOnePersistPeriod) {
    Board b;
    b.boot();
    b.run(36 * 60 * 60 + 59);  // 59 s of progress not yet persisted
    b.boot();
    b.run(kThreeDays);
    ASSERT_EQ(b.expiryTimes.size(), 1u);
    EXPECT_GE(b.expiryTimes[0], kTrip - kPeriod);
    EXPECT_LT(b.expiryTimes[0], kTrip);
}

TEST(CommandLossTimerTest, RebootLoopStillTrips) {
    // Reboot every 10 minutes for the whole period; the RAM-only counter never got past 600 s.
    Board b;
    b.boot();
    while (b.expiryTimes.empty() && b.uptime < 2ull * kThreeDays) {
        b.run(10 * 60);
        b.boot();
    }
    ASSERT_EQ(b.expiryTimes.size(), 1u);
    EXPECT_LE(b.expiryTimes[0], static_cast<std::uint64_t>(kThreeDays));
}

TEST(CommandLossTimerTest, SubPeriodRebootLoopStillTrips) {
    // Reboot every 30 s, so no boot ever reaches a periodic write.
    Board b;
    b.boot();
    while (b.expiryTimes.empty() && b.uptime < 2ull * kThreeDays) {
        b.run(30);
        b.boot();
    }
    ASSERT_EQ(b.expiryTimes.size(), 1u);
    EXPECT_LE(b.expiryTimes[0], static_cast<std::uint64_t>(kThreeDays));
}

TEST(CommandLossTimerTest, ResetPersistsZeroOnNextTickNotFromThePacket) {
    Board b;
    b.boot();
    b.run(125);
    const std::size_t writesBefore = b.writeTimes.size();

    for (int i = 0; i < 5; i++) {
        b.timer.reset();
    }
    EXPECT_EQ(b.writeTimes.size(), writesBefore) << "reset() must not write";

    b.tick();
    ASSERT_EQ(b.writeTimes.size(), writesBefore + 1);
    EXPECT_EQ(b.writeValues.back(), 0u);

    // Further packets while the stored value is already 0 cost no writes
    for (int i = 0; i < 30; i++) {
        b.timer.reset();
        b.tick();
    }
    EXPECT_EQ(b.writeTimes.size(), writesBefore + 1);
}

TEST(CommandLossTimerTest, BootWritesOnce) {
    Board b;
    b.boot();
    ASSERT_EQ(b.writeTimes.size(), 1u);
    EXPECT_EQ(b.writeValues[0], kPeriod);
    b.run(kPeriod - 1);
    EXPECT_EQ(b.writeTimes.size(), 1u);
}

TEST(CommandLossTimerTest, SilenceWritesExactlyOncePerPeriod) {
    Board b;
    b.boot();
    b.run(24 * 60 * 60);
    ASSERT_EQ(b.writeTimes.size(), 1u + 24u * 60u) << "boot write plus one per period";
    for (std::size_t i = 1; i < b.writeTimes.size(); i++) {
        ASSERT_EQ(b.writeTimes[i] - b.writeTimes[i - 1], kPeriod);
    }
}

TEST(CommandLossTimerTest, FrequentPacketsDoNotRaiseTheWriteRate) {
    for (std::uint32_t packetEvery : {1u, 10u, 59u, 61u, 119u}) {
        Board b;
        b.boot();
        b.run(600);  // stored value is non-zero before traffic starts
        const std::size_t writesBefore = b.writeTimes.size();
        const std::uint64_t window = 24 * 60 * 60;
        for (std::uint64_t t = 1; t <= window; t++) {
            if (t % packetEvery == 0) {
                b.timer.reset();
            }
            b.tick();
        }
        const std::size_t writes = b.writeTimes.size() - writesBefore;
        // At most one periodic write per period plus one reset write per period
        EXPECT_LE(writes, 2 * window / kPeriod + 1) << "packetEvery=" << packetEvery;
        if (packetEvery < kPeriod) {
            EXPECT_EQ(writes, 1u) << "packetEvery=" << packetEvery;
        }
    }
}

TEST(CommandLossTimerTest, MissingFileStartsAtOnePeriod) {
    Board b;
    b.boot();
    EXPECT_EQ(b.timer.elapsed(), kPeriod);
}

TEST(CommandLossTimerTest, ShortOrCorruptRecordIsRejected) {
    std::uint8_t record[CommandLossTimer::RECORD_SIZE];
    CommandLossTimer::encode(123456, record);
    std::uint32_t value = 0;
    ASSERT_TRUE(CommandLossTimer::decode(record, sizeof(record), value));
    EXPECT_EQ(value, 123456u);

    value = 7;
    EXPECT_FALSE(CommandLossTimer::decode(nullptr, sizeof(record), value));
    EXPECT_FALSE(CommandLossTimer::decode(record, 0, value));
    EXPECT_FALSE(CommandLossTimer::decode(record, 4, value));
    EXPECT_FALSE(CommandLossTimer::decode(record, sizeof(record) - 1, value));
    EXPECT_EQ(value, 7u);

    for (std::size_t byte = 0; byte < sizeof(record); byte++) {
        std::uint8_t flipped[CommandLossTimer::RECORD_SIZE];
        std::copy(record, record + sizeof(record), flipped);
        flipped[byte] ^= 0x10;
        EXPECT_FALSE(CommandLossTimer::decode(flipped, sizeof(flipped), value)) << "byte " << byte;
    }

    std::uint8_t erased[CommandLossTimer::RECORD_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_FALSE(CommandLossTimer::decode(erased, sizeof(erased), value));
    std::uint8_t zeroed[CommandLossTimer::RECORD_SIZE] = {};
    EXPECT_FALSE(CommandLossTimer::decode(zeroed, sizeof(zeroed), value));
}

TEST(CommandLossTimerTest, CorruptFileOnBootStartsAtOnePeriod) {
    Board b;
    b.boot();
    b.run(kThreeDays - 120);
    ASSERT_EQ(b.file.size(), sizeof(std::uint8_t[CommandLossTimer::RECORD_SIZE]));
    b.file[2] ^= 0x01;
    b.boot();
    EXPECT_EQ(b.timer.elapsed(), kPeriod);
    b.run(kThreeDays);
    ASSERT_EQ(b.expiryTimes.size(), 1u);
    EXPECT_EQ(b.expiryTimes[0], (kThreeDays - 120) + kTrip);
}

TEST(CommandLossTimerTest, ExpiryLatchesUntilAuthenticatedPacket) {
    Board b;
    b.limit = 300;
    b.boot();
    b.run(3000);
    ASSERT_EQ(b.expiryTimes.size(), 1u);
    EXPECT_EQ(b.expiryTimes[0], 300u - kPeriod);

    b.timer.reset();
    b.run(300);
    ASSERT_EQ(b.expiryTimes.size(), 2u);
}

TEST(CommandLossTimerTest, RebootAfterExpiryWaitsAFullLimitLessTheBootCredit) {
    // The expiry stops the watchdog to power cycle; the next boot must not trip immediately.
    Board b;
    b.limit = 600;
    b.boot();
    b.run(600 - kPeriod);
    ASSERT_EQ(b.expiryTimes.size(), 1u);
    b.run(30);  // watchdog power cycle lands
    b.boot();
    b.run(600 - kPeriod - 1);
    EXPECT_EQ(b.expiryTimes.size(), 1u);
    b.run(1);
    EXPECT_EQ(b.expiryTimes.size(), 2u);
}
