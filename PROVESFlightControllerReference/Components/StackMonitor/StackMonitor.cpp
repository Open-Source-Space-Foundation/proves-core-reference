// ======================================================================
// \title  StackMonitor.cpp
// \brief  cpp file for StackMonitor component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/StackMonitor/StackMonitor.hpp"

#include <zephyr/kernel.h>

namespace Components {

namespace {

//! k_thread_foreach callback: append this thread's stack usage to the
//! ThreadStackSampleSet pointed to by userData. Zephyr runs this callback
//! with the thread-monitor spinlock held and IRQs locked, so it must not
//! allocate or block: it only reads the stack high-water mark, does a
//! bounded string copy of the name, and stores integers into a fixed slot
//! (ThreadStackSampleSet::add). Keeps all Zephyr calls out of
//! StackMonitorCore, which stays pure.
void appendThreadSample(const struct k_thread* thread, void* userData) {
    auto* samples = static_cast<ThreadStackSampleSet*>(userData);

    std::size_t unusedBytes = 0;
    if (k_thread_stack_space_get(thread, &unusedBytes) != 0) {
        // Couldn't read this thread's stack info (e.g. mid-teardown); skip
        // it rather than report a bogus sample. The next tick will pick it
        // back up if it's still alive.
        return;
    }

    const char* name = "";
#if defined(CONFIG_THREAD_NAME)
    // thread is logically read-only here, but k_thread_name_get takes a
    // non-const k_tid_t; the callback signature is fixed by
    // k_thread_foreach and always hands us a live thread object.
    name = k_thread_name_get(const_cast<k_tid_t>(thread));
#endif

    (void)samples->add(name, static_cast<std::uint32_t>(thread->stack_info.size),
                       static_cast<std::uint32_t>(unusedBytes));
}

}  // namespace

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StackMonitor ::StackMonitor(const char* const compName)
    : StackMonitorComponentBase(compName), m_core(WARN_THRESHOLD_PERCENT), m_samples(), m_result() {}

StackMonitor ::~StackMonitor() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void StackMonitor ::run_handler(FwIndexType portNum, U32 context) {
    this->m_samples.clear();
    k_thread_foreach(&appendThreadSample, &this->m_samples);

    this->m_core.tick(this->m_samples, this->m_result);

    this->tlmWrite_MinFreeBytes(this->m_result.summary.worstThreadFreeBytes);
    this->tlmWrite_WorstThread(Fw::TlmString(this->m_result.summary.worstThreadName));
    this->tlmWrite_ThreadsBelowThreshold(this->m_result.summary.threadsBelowThreshold);
    this->tlmWrite_SampleOverflow(this->m_result.summary.overflowed);

    for (std::uint32_t i = 0; i < this->m_result.warningCount; i++) {
        const StackWarning& warning = this->m_result.newWarnings[i];
        this->log_WARNING_HI_StackLow(Fw::LogStringArg(warning.name), warning.freeBytes, warning.sizeBytes);
    }
    for (std::uint32_t i = 0; i < this->m_result.recoveryCount; i++) {
        this->log_ACTIVITY_HI_StackRecovered(Fw::LogStringArg(this->m_result.newRecoveries[i].name));
    }
}

}  // namespace Components
