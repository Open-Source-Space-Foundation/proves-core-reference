// ======================================================================
// \title  StackMonitorCore.cpp
// \brief  cpp file for StackMonitorCore pure-logic class
// ======================================================================

#include "PROVESFlightControllerReference/Components/StackMonitor/StackMonitorCore.hpp"

namespace Components {

namespace {

//! Free bytes, clamped so a corrupt/odd sample can never read as more free
//! than the thread's total stack size.
std::uint32_t clampedFreeBytes(const ThreadStackSample& sample) {
    return (sample.freeBytes > sample.sizeBytes) ? sample.sizeBytes : sample.freeBytes;
}

//! Percent of the thread's stack currently free, 0-100.
std::uint32_t freePercent(const ThreadStackSample& sample) {
    if (sample.sizeBytes == 0) {
        return 0;
    }
    return (clampedFreeBytes(sample) * 100) / sample.sizeBytes;
}

}  // namespace

StackMonitorCore::StackMonitorCore(std::uint32_t warnThresholdPercent) : m_warnThresholdPercent(warnThresholdPercent) {}

StackMonitorTickResult StackMonitorCore::tick(const std::vector<ThreadStackSample>& samples) {
    StackMonitorTickResult result;

    bool haveWorst = false;
    std::uint32_t worstFreePercent = 0;

    for (const auto& sample : samples) {
        std::uint32_t fPercent = freePercent(sample);

        if (!haveWorst || fPercent < worstFreePercent) {
            haveWorst = true;
            worstFreePercent = fPercent;
            result.summary.worstThreadName = sample.name;
            result.summary.worstThreadFreeBytes = sample.freeBytes;
            result.summary.worstThreadUsedPercent = 100 - fPercent;
        }

        bool isBelowThreshold = fPercent < m_warnThresholdPercent;
        if (isBelowThreshold) {
            result.summary.threadsBelowThreshold++;
        }

        bool wasWarned = m_warned[sample.name];
        if (isBelowThreshold && !wasWarned) {
            result.newWarnings.push_back({sample.name, sample.freeBytes, sample.sizeBytes});
            m_warned[sample.name] = true;
        } else if (!isBelowThreshold && wasWarned) {
            result.newRecoveries.push_back({sample.name});
            m_warned[sample.name] = false;
        }
    }

    return result;
}

}  // namespace Components
