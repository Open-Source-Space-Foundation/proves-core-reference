module Components {
    @ Program-wide stack-usage watchdog. Walks all live Zephyr threads once per
    @ tick (via ThreadInfoProviderIf) and reports per-thread stack high-water
    @ telemetry, warning when any thread's free stack drops below a configurable
    @ percent of its own size and clearing on recovery.
    @ S-Band reintegration plan, PR 1 / Slice 1.1 (D6).
    passive component StackMonitor {

        @ Port receiving calls from the rate group
        sync input port run: Svc.Sched

        @ Free bytes remaining on the thread under the most stack pressure this tick
        telemetry MinFreeBytes: U32

        @ Name of the thread under the most stack pressure this tick
        telemetry WorstThread: string size 32

        @ Count of threads currently below the warn threshold
        telemetry ThreadsBelowThreshold: U32

        @ Event logged when a thread's free stack drops below its warn threshold
        event StackLow(
            thread: string size 32 @< Name of the thread
            freeBytes: U32 @< Free bytes remaining on the thread's stack
            sizeBytes: U32 @< Total size of the thread's stack
        ) \
            severity warning high \
            format "Thread {} stack low: {} of {} bytes free"

        @ Event logged when a previously-low thread recovers above its warn threshold
        event StackRecovered(
            thread: string size 32 @< Name of the thread
        ) \
            severity activity high \
            format "Thread {} stack recovered"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

    }
}
