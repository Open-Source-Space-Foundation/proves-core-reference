// ======================================================================
// \title  StackMonitorCore.hpp
// \brief  hpp file for StackMonitorCore pure-logic class
// ======================================================================

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Components {

//! One thread's stack usage as sampled for a single tick.
struct ThreadStackSample {
    std::string name;
    std::uint32_t sizeBytes;
    std::uint32_t freeBytes;
};

//! Per-tick summary across all sampled threads.
struct StackMonitorSummary {
    std::string worstThreadName;
    std::uint32_t worstThreadFreeBytes = 0;
    std::uint32_t worstThreadUsedPercent = 0;
    std::uint32_t threadsBelowThreshold = 0;
};

//! A thread that just crossed below its warn threshold this tick.
struct StackWarning {
    std::string name;
    std::uint32_t freeBytes;
    std::uint32_t sizeBytes;
};

//! A thread that just crossed back above its warn threshold this tick.
struct StackRecovery {
    std::string name;
};

//! Result of processing one tick of thread stack samples.
struct StackMonitorTickResult {
    StackMonitorSummary summary;
    std::vector<StackWarning> newWarnings;
    std::vector<StackRecovery> newRecoveries;
};

//! Pure logic core for the StackMonitor component.
//!
//! Given a snapshot of per-thread stack usage taken once per tick, computes a
//! summary of the thread under the most stack pressure and (in a later slice)
//! warn/clear decisions. Host-compilable: no Zephyr or F Prime autocode
//! dependencies, matching the pattern used by DetumbleManager::BDot.
class StackMonitorCore {
  public:
    //! \param warnThresholdPercent warn when a thread's free stack drops
    //!        below this percent of its own size.
    explicit StackMonitorCore(std::uint32_t warnThresholdPercent);

    //! Process one tick's worth of thread stack samples.
    StackMonitorTickResult tick(const std::vector<ThreadStackSample>& samples);

  private:
    std::uint32_t m_warnThresholdPercent;

    //! Latched warn state per thread name; true while a thread is below
    //! threshold and hasn't yet recovered. Kept across ticks so a thread
    //! that stays below threshold isn't re-warned every tick, and a thread
    //! that disappears from the sample set simply stops being updated
    //! (no assert, no crash) until it reappears.
    std::unordered_map<std::string, bool> m_warned;
};

}  // namespace Components
