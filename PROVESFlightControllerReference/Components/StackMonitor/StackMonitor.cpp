// ======================================================================
// \title  StackMonitor.cpp
// \brief  cpp file for StackMonitor component implementation class
// ======================================================================

#include "PROVESFlightControllerReference/Components/StackMonitor/StackMonitor.hpp"

#include <zephyr/kernel.h>

namespace Components {

namespace {

//! k_thread_foreach callback: append this thread's stack usage to the
//! std::vector<ThreadStackSample> pointed to by userData. Keeps all Zephyr
//! calls (and their quirks) out of StackMonitorCore, which stays pure.
void appendThreadSample(const struct k_thread* thread, void* userData) {
    auto* samples = static_cast<std::vector<ThreadStackSample>*>(userData);

    // thread is logically read-only here, but the Zephyr stack-space API
    // takes a non-const k_tid_t; the callback signature is fixed by
    // k_thread_foreach and always hands us a live, non-const thread object.
    k_tid_t tid = const_cast<k_tid_t>(thread);

    std::size_t unusedBytes = 0;
    if (k_thread_stack_space_get(thread, &unusedBytes) != 0) {
        // Couldn't read this thread's stack info (e.g. mid-teardown); skip
        // it rather than report a bogus sample. The next tick will pick it
        // back up if it's still alive.
        return;
    }

    ThreadStackSample sample;
#if defined(CONFIG_THREAD_NAME)
    const char* name = k_thread_name_get(tid);
    sample.name = (name != nullptr) ? name : "";
#endif
    sample.sizeBytes = static_cast<std::uint32_t>(thread->stack_info.size);
    sample.freeBytes = static_cast<std::uint32_t>(unusedBytes);

    samples->push_back(sample);
}

}  // namespace

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StackMonitor ::StackMonitor(const char* const compName)
    : StackMonitorComponentBase(compName), m_core(WARN_THRESHOLD_PERCENT) {}

StackMonitor ::~StackMonitor() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void StackMonitor ::run_handler(FwIndexType portNum, U32 context) {
    std::vector<ThreadStackSample> samples;
    k_thread_foreach(&appendThreadSample, &samples);

    StackMonitorTickResult result = this->m_core.tick(samples);

    this->tlmWrite_MinFreeBytes(result.summary.worstThreadFreeBytes);
    this->tlmWrite_WorstThread(Fw::TlmString(result.summary.worstThreadName.c_str()));
    this->tlmWrite_ThreadsBelowThreshold(result.summary.threadsBelowThreshold);

    for (const auto& warning : result.newWarnings) {
        this->log_WARNING_HI_StackLow(Fw::LogStringArg(warning.name.c_str()), warning.freeBytes, warning.sizeBytes);
    }
    for (const auto& recovery : result.newRecoveries) {
        this->log_ACTIVITY_HI_StackRecovered(Fw::LogStringArg(recovery.name.c_str()));
    }
}

}  // namespace Components
