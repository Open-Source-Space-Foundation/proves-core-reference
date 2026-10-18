// Research instrumentation for #601 (throwaway branch).
//
// Probe threads: one per candidate uspRadio priority. Each sleeps on an absolute tick
// deadline and records how late it woke. Lateness at priority p is the time the CPU
// was held by threads of priority < p (plus ISRs and tick granularity). This is the
// quantity the #589 host sim calls a "burst" when it models RX-harvest delay.
//
// Reporter thread (priority 14): on REPORT, emits probe stats, handler max-holds and
// Zephyr per-thread usage (CONFIG_SCHED_THREAD_USAGE_ANALYSIS peak/avg run lengths).

#include "PROVESFlightControllerReference/Components/SchedProbe/SchedProbe.hpp"
#include "PROVESFlightControllerReference/Components/SchedProbe/MaxHold.hpp"

#include <zephyr/kernel.h>
#include <cstring>
#include <cstdio>
#include <initializer_list>

namespace Components {

MaxHold g_modeManagerRun = {"mm.run(rg1Hz)", 0, 0};
MaxHold g_modeManagerForceSafe = {"mm.forceSafe", 0, 0};
MaxHold g_modeManagerCmd = {"mm.cmd(cmdDisp)", 0, 0};

namespace {

constexpr U32 CYC_PER_US = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / 1000000U;
constexpr U32 CYC_PER_TICK = CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / CONFIG_SYS_CLOCK_TICKS_PER_SEC;
constexpr U32 PROBE_PERIOD_US = 4000;
constexpr U32 PROBE_PERIOD_TICKS = PROBE_PERIOD_US * CONFIG_SYS_CLOCK_TICKS_PER_SEC / 1000000U;
constexpr int REPORT_PACING_MS = 40;

struct ProbeStat {
    I8 prio;
    volatile U32 samples;
    volatile U32 max_late_us;
    volatile U32 over_1ms;
    volatile U32 over_5ms;
    volatile U32 over_20ms;
};

ProbeStat s_probes[] = {{3, 0, 0, 0, 0, 0}, {5, 0, 0, 0, 0, 0}, {7, 0, 0, 0, 0, 0}, {8, 0, 0, 0, 0, 0}, {9, 0, 0, 0, 0, 0}, {10, 0, 0, 0, 0, 0}, {11, 0, 0, 0, 0, 0}};
constexpr unsigned NUM_PROBES = sizeof(s_probes) / sizeof(s_probes[0]);

K_THREAD_STACK_ARRAY_DEFINE(s_probeStacks, NUM_PROBES, 1024);
::k_thread s_probeThreads[NUM_PROBES];
K_THREAD_STACK_DEFINE(s_reporterStack, 2048);
::k_thread s_reporterThread;

// Thread-switch ring filled by the CONFIG_TRACING_USER hook. The prio-11 probe copies it on its first wake later
// than SWITCH_CAPTURE_US so REPORT can name the threads that held the CPU during that gap.
constexpr unsigned SWITCH_RING = 64;
constexpr unsigned NUM_CAPTURES = 3;
constexpr U32 SWITCH_CAPTURE_US = 8000;
struct SwitchRec { k_tid_t thread; U32 cycles; };
SwitchRec s_ring[SWITCH_RING];
volatile unsigned s_ringHead = 0;
struct Capture { SwitchRec rec[SWITCH_RING]; U32 wakeCycles; U32 lateUs; };
Capture s_capture[NUM_CAPTURES];
volatile unsigned s_captured = 0;

extern "C" void sys_trace_thread_switched_in_user(void) {
    unsigned h = s_ringHead;
    s_ring[h % SWITCH_RING] = {k_current_get(), k_cycle_get_32()};
    s_ringHead = h + 1;
}
extern "C" void sys_trace_thread_switched_out_user(void) {}
extern "C" void sys_trace_isr_enter_user(void) {}
extern "C" void sys_trace_isr_exit_user(void) {}
extern "C" void sys_trace_isr_exit_to_scheduler_user(void) {}
extern "C" void sys_trace_idle_user(void) {}
extern "C" void sys_trace_idle_exit_user(void) {}

void probeEntry(void* arg, void*, void*) {
    ProbeStat* st = static_cast<ProbeStat*>(arg);
    int64_t next = k_uptime_ticks() + PROBE_PERIOD_TICKS;
    for (;;) {
        k_sleep(K_TIMEOUT_ABS_TICKS(next));
        const U32 expected = static_cast<U32>(next) * CYC_PER_TICK;
        const U32 late_us = (k_cycle_get_32() - expected) / CYC_PER_US;
        st->samples++;
        if (late_us > st->max_late_us) {
            st->max_late_us = late_us;
        }
        if (late_us > 1000) {
            st->over_1ms++;
        }
        if (late_us > 5000) {
            st->over_5ms++;
        }
        if (late_us > 20000) {
            st->over_20ms++;
        }
        if (st->prio == 11 && late_us > SWITCH_CAPTURE_US && s_captured < NUM_CAPTURES) {
            unsigned key = irq_lock();
            Capture& cap = s_capture[s_captured];
            unsigned h = s_ringHead;
            for (unsigned i = 0; i < SWITCH_RING; i++) {
                cap.rec[i] = s_ring[(h + i) % SWITCH_RING];
            }
            cap.wakeCycles = k_cycle_get_32();
            cap.lateUs = late_us;
            s_captured++;
            irq_unlock(key);
        }
        // A wake later than one period would make the next deadline already past; restart the schedule.
        if (late_us > PROBE_PERIOD_US) {
            next = k_uptime_ticks();
        }
        next += PROBE_PERIOD_TICKS;
    }
}

void resetProbes() {
    for (ProbeStat& st : s_probes) {
        st.samples = 0;
        st.max_late_us = 0;
        st.over_1ms = 0;
        st.over_5ms = 0;
        st.over_20ms = 0;
    }
    for (MaxHold* h : {&g_modeManagerRun, &g_modeManagerForceSafe, &g_modeManagerCmd}) {
        h->max_us = 0;
        h->count = 0;
    }
    s_captured = 0;
}

}  // namespace

SchedProbe::SchedProbe(const char* const compName) : SchedProbeComponentBase(compName) {}

SchedProbe::~SchedProbe() {}

void SchedProbe::start() {
    for (unsigned i = 0; i < NUM_PROBES; i++) {
        k_tid_t tid = k_thread_create(&s_probeThreads[i], s_probeStacks[i], K_THREAD_STACK_SIZEOF(s_probeStacks[i]), probeEntry,
                                      &s_probes[i], nullptr, nullptr, s_probes[i].prio, 0, K_NO_WAIT);
        char name[16];
        snprintf(name, sizeof(name), "probe_p%d", s_probes[i].prio);
        k_thread_name_set(tid, name);
    }
    k_tid_t rep = k_thread_create(&s_reporterThread, s_reporterStack, K_THREAD_STACK_SIZEOF(s_reporterStack), reporterEntry, this,
                                  nullptr, nullptr, 14, 0, K_NO_WAIT);
    k_thread_name_set(rep, "probe_reporter");
}

void SchedProbe::REPORT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->m_reportRequested = true;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void SchedProbe::RESET_PROBES_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    resetProbes();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void SchedProbe::reporterEntry(void* self, void*, void*) {
    SchedProbe* probe = static_cast<SchedProbe*>(self);
    for (;;) {
        k_sleep(K_MSEC(100));
        if (probe->m_reportRequested) {
            probe->m_reportRequested = false;
            probe->report();
        }
    }
}

void SchedProbe::threadCb(const ::k_thread* thread, void* self) {
    static_cast<SchedProbe*>(self)->reportThread(const_cast<k_tid_t>(thread));
}

void SchedProbe::reportThread(k_tid_t thread) {
    k_thread_runtime_stats_t stats;
    std::memset(&stats, 0, sizeof(stats));
    k_thread_runtime_stats_get(thread, &stats);
    k_thread_runtime_stats_t all;
    std::memset(&all, 0, sizeof(all));
    k_thread_runtime_stats_all_get(&all);
    const char* rawName = k_thread_name_get(thread);
    Fw::String name(rawName != nullptr ? rawName : "?");
    const U64 permille = all.execution_cycles ? (stats.total_cycles * 1000U) / all.execution_cycles : 0;
    this->log_ACTIVITY_HI_ThreadReport(name, static_cast<I8>(k_thread_priority_get(thread)),
                                       static_cast<U32>(stats.peak_cycles / CYC_PER_US), static_cast<U32>(stats.average_cycles / CYC_PER_US),
                                       static_cast<U32>(stats.total_cycles / (CYC_PER_US * 1000U)), static_cast<U16>(permille));
    k_sleep(K_MSEC(REPORT_PACING_MS));
}

void SchedProbe::report() {
    for (const ProbeStat& st : s_probes) {
        this->log_ACTIVITY_HI_ProbeReport(st.prio, PROBE_PERIOD_US, st.samples, st.max_late_us, st.over_1ms, st.over_5ms, st.over_20ms);
        k_sleep(K_MSEC(REPORT_PACING_MS));
    }
    for (const MaxHold* h : {&g_modeManagerRun, &g_modeManagerForceSafe, &g_modeManagerCmd}) {
        this->log_ACTIVITY_HI_HandlerReport(Fw::String(h->name), h->max_us, h->count);
        k_sleep(K_MSEC(REPORT_PACING_MS));
    }
    k_thread_foreach_unlocked(threadCb, this);
    for (unsigned c = 0; c < s_captured; c++) {
        const Capture& cap = s_capture[c];
        this->log_ACTIVITY_HI_SwitchCaptureHeader(cap.lateUs, SWITCH_RING);
        k_sleep(K_MSEC(REPORT_PACING_MS));
        for (unsigned i = 0; i < SWITCH_RING; i++) {
            const SwitchRec& r = cap.rec[i];
            if (r.thread == nullptr) {
                continue;
            }
            const U32 endCycles = (i + 1 < SWITCH_RING) ? cap.rec[i + 1].cycles : cap.wakeCycles;
            const I32 beforeWakeUs = static_cast<I32>(cap.wakeCycles - r.cycles) / static_cast<I32>(CYC_PER_US);
            const U32 ranUs = (endCycles - r.cycles) / CYC_PER_US;
            if (ranUs < 20) {
                continue;  // sub-20 us hops (the 1 kHz main tick, probe wakes) only add noise to the event stream
            }
            const char* rawName = k_thread_name_get(r.thread);
            this->log_ACTIVITY_HI_SwitchRec(i, Fw::String(rawName != nullptr ? rawName : "?"), static_cast<I8>(k_thread_priority_get(r.thread)),
                                            beforeWakeUs, ranUs);
            k_sleep(K_MSEC(REPORT_PACING_MS));
        }
    }
    k_thread_runtime_stats_t all;
    std::memset(&all, 0, sizeof(all));
    k_thread_runtime_stats_all_get(&all);
    U64 probeCycles = 0;
    for (unsigned i = 0; i < NUM_PROBES; i++) {
        k_thread_runtime_stats_t s;
        std::memset(&s, 0, sizeof(s));
        k_thread_runtime_stats_get(&s_probeThreads[i], &s);
        probeCycles += s.total_cycles;
    }
    this->log_ACTIVITY_HI_CpuReport(static_cast<U32>(k_uptime_get()), static_cast<U32>(all.total_cycles / (CYC_PER_US * 1000U)),
                                    static_cast<U32>(probeCycles / (CYC_PER_US * 1000U)));
    k_sleep(K_MSEC(REPORT_PACING_MS));
    this->log_ACTIVITY_HI_ReportDone();
}

}  // namespace Components
