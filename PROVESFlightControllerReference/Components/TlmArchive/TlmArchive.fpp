module Components {
    @ Stores telemetry generated before antenna deployment
    passive component TlmArchive {
        @ Telemetry packet to enqueue for deferred archival
        sync input port comIn: Fw.Com

        @ Drains one queued telemetry packet and performs filesystem work
        sync input port run: Svc.Sched

        @ Port for checking whether antenna deployment has completed
        output port deploymentStateGet: Components.GetDeploymentState

        @ Report when file write has started
        event WriteStart severity activity low format "Beginning telemetry archival to pre_deployment.csv" throttle 1

        @ Reports that a packet could not be enqueued because the queue is full
        event QueueFull(
            capacity: FwSizeType @< Maximum number of queued telemetry packets
        ) severity warning high format "Telemetry archive packet queue is full at {} packets" throttle 1

        @ Reports archive initialization, size, and open failures
        event FileError(
            operation: string @< Filesystem operation that failed
        ) severity warning high \
          format "Pre-deployment telemetry archive operation failed: {}"

        @ Reports failed and incomplete archive writes
        event WriteError(
            status: Os.FileStatus @< File write status
            requested: FwSizeType @< Requested byte count
            written: FwSizeType @< Reported byte count
        ) severity warning high \
          format "Pre-deployment telemetry archive write failed: status {}, requested {}, wrote {}"

        @ Reports when telemetry archiving is disabled due to hitting the failure limit
        event FailureLimitReached(
            count: I8
        ) severity warning high format "{} filesystem failures counted; disabling further telemetry writes." throttle 1

        @ Reports when telemetry archiving is disabled due to antennas being deployed
        event AntennasDeployed() severity warning low format "Antennas deployed; disabling further telemetry writes." throttle 1

        @ Reports when telemetry archiving is disabled due to pre_deployment.csv hitting the size limit
        event SizeLimitReached(
            maxSize: FwSizeType
        ) severity warning low format "pre_deployment.csv file size limit of {}b reached; disabling further telemetry writes." throttle 1

        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut
    }
}
