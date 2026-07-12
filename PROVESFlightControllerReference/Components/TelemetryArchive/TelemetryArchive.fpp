module Components {
    enum TelemetryArchiveMode {
        DEPLOYMENT
        ROLLING
    }

    @ Passively archives packetized telemetry to bounded filesystem segments
    passive component TelemetryArchive {
        @ Packetized telemetry from the telemetry ComSplitter
        sync input port comIn: Fw.Com

        @ Irreversible notification that antenna deployment was persisted
        sync input port deploymentComplete: Fw.Signal

        @ Enable telemetry archiving and retry after a paused state
        sync command ENABLE()

        @ Disable telemetry archiving and close the current segment
        sync command DISABLE()

        @ Flush and close the current segment for downlink
        sync command CLOSE_SEGMENT()

        @ Delete all rolling segments while preserving deployment telemetry
        sync command PURGE_ROLLING()

        @ Emit the current archive mode and counters
        sync command GET_STATUS()

        telemetry Mode: TelemetryArchiveMode update on change
        telemetry Enabled: bool update on change
        telemetry Paused: bool update on change
        telemetry RecordsStored: U32
        telemetry BytesStored: U32
        telemetry RecordsDropped: U32
        telemetry RollingBytesPruned: U32
        telemetry LastWriteDurationUs: U32
        telemetry MaxWriteDurationUs: U32
        telemetry FreeSpace: FwSizeType

        event SegmentOpened(path: string size 96, isProtected: bool) severity activity low \
            format "Telemetry archive opened {} protected={}"
        event SegmentClosed(path: string size 96) severity activity high \
            format "Telemetry archive closed {}"
        event ArchivePaused(reason: string size 48) severity warning high \
            format "Telemetry archive paused: {}" throttle 5
        event DeploymentCaptureComplete(path: string size 96) severity activity high \
            format "Permanent deployment telemetry closed: {}"
        event ArchiveStatus(mode: TelemetryArchiveMode, enabled: bool, paused: bool, records: U32, bytes: U32) \
            severity activity low format "Telemetry archive mode={} enabled={} paused={} records={} bytes={}"

        time get port timeCaller
        command reg port cmdRegOut
        command recv port cmdIn
        command resp port cmdResponseOut
        telemetry port tlmOut
        text event port logTextOut
        event port logOut
    }
}
