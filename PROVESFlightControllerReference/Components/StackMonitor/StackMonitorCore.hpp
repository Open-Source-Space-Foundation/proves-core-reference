// ======================================================================
// \title  StackMonitorCore.hpp
// \brief  hpp file for StackMonitorCore pure-logic class
// ======================================================================

#pragma once

#include <cstdint>

namespace Components {

//! Fixed capacities: no heap allocation anywhere in the stack-monitoring
//! path (per-tick heap churn is not flight-safe; see issue #122 and the
//! S-Band reintegration plan). MAX_NAME_LEN matches Zephyr's default
//! CONFIG_THREAD_MAX_NAME_LEN; MAX_THREADS covers the 25-slot dynamic
//! thread pool plus static threads with margin.
static constexpr std::uint32_t STACK_MONITOR_MAX_NAME_LEN = 32;
static constexpr std::uint32_t STACK_MONITOR_MAX_THREADS = 32;

//! One thread's stack usage as sampled for a single tick. POD with a
//! fixed-size name buffer: safe to fill inside k_thread_foreach's
//! spinlocked callback (integer stores + bounded string copy only).
struct ThreadStackSample {
    char name[STACK_MONITOR_MAX_NAME_LEN];
    std::uint32_t sizeBytes;
    std::uint32_t freeBytes;
};

//! Caller-owned, fixed-capacity set of samples for one tick.
//! add() performs only a bounds check, a bounded string copy, and integer
//! stores -- no allocation of any kind -- so it is safe to call from the
//! k_thread_foreach callback, which Zephyr runs with the thread-monitor
//! spinlock held and IRQs locked.
struct ThreadStackSampleSet {
    ThreadStackSample samples[STACK_MONITOR_MAX_THREADS];
    std::uint32_t count = 0;
    bool overflowed = false;

    //! Reset to empty (count/overflowed only; sample slots are overwritten on add).
    void clear();

    //! Append a sample. When the set is full the sample is dropped and the
    //! overflowed flag is set (no silent truncation); returns false in that case.
    bool add(const char* name, std::uint32_t sizeBytes, std::uint32_t freeBytes);
};

//! Per-tick summary across all sampled threads.
struct StackMonitorSummary {
    char worstThreadName[STACK_MONITOR_MAX_NAME_LEN] = {0};
    std::uint32_t worstThreadFreeBytes = 0;
    std::uint32_t worstThreadUsedPercent = 0;
    std::uint32_t threadsBelowThreshold = 0;
    bool overflowed = false;  //!< True when this tick saw more threads than capacity
};

//! A thread that just crossed below its warn threshold this tick.
struct StackWarning {
    char name[STACK_MONITOR_MAX_NAME_LEN];
    std::uint32_t freeBytes;
    std::uint32_t sizeBytes;
};

//! A thread that just crossed back above its warn threshold this tick.
struct StackRecovery {
    char name[STACK_MONITOR_MAX_NAME_LEN];
};

//! Result of processing one tick of thread stack samples. Fixed-size;
//! intended to live as long-lived (member) storage, not on a small
//! thread's stack.
struct StackMonitorTickResult {
    StackMonitorSummary summary;
    StackWarning newWarnings[STACK_MONITOR_MAX_THREADS];
    std::uint32_t warningCount = 0;
    StackRecovery newRecoveries[STACK_MONITOR_MAX_THREADS];
    std::uint32_t recoveryCount = 0;
};

//! Pure logic core for the StackMonitor component.
//!
//! Given a snapshot of per-thread stack usage taken once per tick, computes a
//! summary of the thread under the most stack pressure and warn/clear
//! decisions per thread with hysteresis. Host-compilable: no Zephyr or
//! F Prime autocode dependencies, matching the pattern used by
//! DetumbleManager::BDot. All state is fixed-size at construction; the
//! class performs zero heap allocation.
class StackMonitorCore {
  public:
    //! \param warnThresholdPercent warn when a thread's free stack drops
    //!        below this percent of its own size.
    explicit StackMonitorCore(std::uint32_t warnThresholdPercent);

    //! Process one tick's worth of thread stack samples, writing into the
    //! caller-provided result (cleared first). No allocation.
    void tick(const ThreadStackSampleSet& sampleSet, StackMonitorTickResult& result);

  private:
    //! Latched warn state, keyed by fixed-size thread name. An entry is
    //! occupied while its thread is below threshold and hasn't yet
    //! recovered; it is freed on recovery. Kept across ticks so a thread
    //! that stays below threshold isn't re-warned every tick, and a thread
    //! that disappears from the sample set simply keeps its entry (no
    //! assert, no crash, no re-warn if it reappears still below).
    //! Linear scan over <=MAX_THREADS entries at 1 Hz.
    struct WarnEntry {
        char name[STACK_MONITOR_MAX_NAME_LEN];
        bool used;
    };

    //! Find the warn-table index for a name, or -1 if absent.
    std::int32_t findWarned(const char* name) const;

    //! Mark a name warned (no-op if the table is unexpectedly full; the
    //! warning event itself is still emitted by tick()).
    void setWarned(const char* name);

    //! Clear a name's warned entry, if present.
    void clearWarned(const char* name);

    std::uint32_t m_warnThresholdPercent;
    WarnEntry m_warned[STACK_MONITOR_MAX_THREADS];
};

}  // namespace Components
