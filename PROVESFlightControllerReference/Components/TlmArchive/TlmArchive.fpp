module Components {
    @ Stores telemetry generated before antenna deployment
    passive component TlmArchive {
        @ Telemetry packet to buffer. Drop new packets while the one-packet mailbox is full.
        sync input port comIn: Fw.Com

        @ Drains the telemetry mailbox and performs filesystem work
        sync input port run: Svc.Sched

        @ Port for checking whether antenna deployment has completed
        output port deploymentStateGet: Components.GetDeploymentState

        @ Reports archive directory and open failures
        event ArchiveFileError(
            operation: string @< Filesystem operation that failed
        ) severity warning high \
          format "Pre-deployment telemetry archive operation failed: {}"

        @ Reports failed and incomplete archive writes
        event ArchiveWriteError(
            status: Os.FileStatus @< File write status
            requested: FwSizeType @< Requested byte count
            written: FwSizeType @< Reported byte count
        ) severity warning high \
          format "Pre-deployment telemetry archive write failed: status {}, requested {}, wrote {}"

        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut
    }
}
