module Components {
    @ Research instrumentation for #601: scheduling-latency probes and Zephyr thread usage reports.
    passive component SchedProbe {

        @ Emit one ProbeReport per probe thread, one HandlerReport per instrumented handler,
        @ one ThreadReport per Zephyr thread and one CpuReport. Paced from a priority-14 thread.
        sync command REPORT()

        @ Zero the probe max-holds and the handler max-holds (Zephyr per-thread peaks cannot be reset).
        sync command RESET_PROBES()

        event ProbeReport(prio: I8, period_us: U32, samples: U32, max_late_us: U32, over_1ms: U32, over_5ms: U32, over_20ms: U32) \
            severity activity high \
            format "probe prio {} period {} us: {} wakes, max late {} us, >1ms {}, >5ms {}, >20ms {}"

        event HandlerReport(name: string size 24, max_us: U32, count: U32) \
            severity activity high \
            format "handler {}: max {} us over {} calls"

        event ThreadReport(name: string size 32, prio: I8, peak_us: U32, avg_us: U32, total_ms: U32, permille: U16) \
            severity activity high \
            format "thread {} prio {}: peak run {} us, avg run {} us, total {} ms, {} permille cpu"

        event CpuReport(uptime_ms: U32, busy_ms: U32, probe_overhead_ms: U32) \
            severity activity high \
            format "cpu: uptime {} ms, non-idle {} ms, probe threads {} ms"

        event GapCaptureHeader(late_us: U32, gap_us: U32, busy_us: U32) \
            severity activity high \
            format "gap: prio-11 probe woke {} us late; {} us since its previous wake, of which {} us non-idle"

        event GapThread(capture: U32, name: string size 32, prio: I8, cpu_us: U32) \
            severity activity high \
            format "gap[{}] {} prio {} used {} us"

        event ReportDone() severity activity high format "report complete"

        time get port timeCaller
        command recv port cmdIn
        command reg port cmdRegOut
        command resp port cmdResponseOut
        event port logOut
        text event port logTextOut
    }
}
