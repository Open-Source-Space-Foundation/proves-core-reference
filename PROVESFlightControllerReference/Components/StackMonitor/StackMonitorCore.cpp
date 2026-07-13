// ======================================================================
// \title  StackMonitorCore.cpp
// \brief  cpp file for StackMonitorCore pure-logic class
// ======================================================================

#include "PROVESFlightControllerReference/Components/StackMonitor/StackMonitorCore.hpp"

#include <cstring>

namespace Components {

namespace {

//! Bounded copy of a thread name into a fixed-size slot, always
//! null-terminated. No allocation.
void copyName(char (&dst)[STACK_MONITOR_MAX_NAME_LEN], const char* src) {
    (void)std::strncpy(dst, (src != nullptr) ? src : "", STACK_MONITOR_MAX_NAME_LEN - 1);
    dst[STACK_MONITOR_MAX_NAME_LEN - 1] = '\0';
}

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

// ----------------------------------------------------------------------
// ThreadStackSampleSet
// ----------------------------------------------------------------------

void ThreadStackSampleSet::clear() {
    this->count = 0;
    this->overflowed = false;
}

bool ThreadStackSampleSet::add(const char* name, std::uint32_t sizeBytes, std::uint32_t freeBytes) {
    if (this->count >= STACK_MONITOR_MAX_THREADS) {
        this->overflowed = true;
        return false;
    }
    ThreadStackSample& slot = this->samples[this->count];
    copyName(slot.name, name);
    slot.sizeBytes = sizeBytes;
    slot.freeBytes = freeBytes;
    this->count++;
    return true;
}

// ----------------------------------------------------------------------
// StackMonitorCore
// ----------------------------------------------------------------------

StackMonitorCore::StackMonitorCore(std::uint32_t warnThresholdPercent)
    : m_warnThresholdPercent(warnThresholdPercent), m_warned() {}

void StackMonitorCore::tick(const ThreadStackSampleSet& sampleSet, StackMonitorTickResult& result) {
    result.summary = StackMonitorSummary();
    result.summary.overflowed = sampleSet.overflowed;
    result.warningCount = 0;
    result.recoveryCount = 0;

    bool haveWorst = false;
    std::uint32_t worstFreePercent = 0;

    for (std::uint32_t i = 0; i < sampleSet.count; i++) {
        const ThreadStackSample& sample = sampleSet.samples[i];
        std::uint32_t fPercent = freePercent(sample);

        if (!haveWorst || fPercent < worstFreePercent) {
            haveWorst = true;
            worstFreePercent = fPercent;
            copyName(result.summary.worstThreadName, sample.name);
            result.summary.worstThreadFreeBytes = sample.freeBytes;
            result.summary.worstThreadUsedPercent = 100 - fPercent;
        }

        bool isBelowThreshold = fPercent < m_warnThresholdPercent;
        if (isBelowThreshold) {
            result.summary.threadsBelowThreshold++;
        }

        bool wasWarned = (this->findWarned(sample.name) >= 0);
        if (isBelowThreshold && !wasWarned) {
            if (result.warningCount < STACK_MONITOR_MAX_THREADS) {
                StackWarning& warning = result.newWarnings[result.warningCount];
                copyName(warning.name, sample.name);
                warning.freeBytes = sample.freeBytes;
                warning.sizeBytes = sample.sizeBytes;
                result.warningCount++;
            }
            this->setWarned(sample.name);
        } else if (!isBelowThreshold && wasWarned) {
            if (result.recoveryCount < STACK_MONITOR_MAX_THREADS) {
                copyName(result.newRecoveries[result.recoveryCount].name, sample.name);
                result.recoveryCount++;
            }
            this->clearWarned(sample.name);
        }
    }
}

std::int32_t StackMonitorCore::findWarned(const char* name) const {
    for (std::uint32_t i = 0; i < STACK_MONITOR_MAX_THREADS; i++) {
        if (this->m_warned[i].used && (std::strncmp(this->m_warned[i].name, name, STACK_MONITOR_MAX_NAME_LEN) == 0)) {
            return static_cast<std::int32_t>(i);
        }
    }
    return -1;
}

void StackMonitorCore::setWarned(const char* name) {
    if (this->findWarned(name) >= 0) {
        return;
    }
    for (std::uint32_t i = 0; i < STACK_MONITOR_MAX_THREADS; i++) {
        if (!this->m_warned[i].used) {
            copyName(this->m_warned[i].name, name);
            this->m_warned[i].used = true;
            return;
        }
    }
    // Table full: the warning event is still emitted by tick(); this
    // thread just isn't latched (it may re-warn on a later tick).
}

void StackMonitorCore::clearWarned(const char* name) {
    std::int32_t index = this->findWarned(name);
    if (index >= 0) {
        this->m_warned[index].used = false;
    }
}

}  // namespace Components
