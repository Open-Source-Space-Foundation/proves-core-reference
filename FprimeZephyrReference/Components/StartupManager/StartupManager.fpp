module Components {
    @ Manages the start-up sequencing
    passive component StartupManager {
        @ Check RTC time diff
        sync input port run: Svc.Sched

        @ Port for sending sequence dispatches
        output port runSequence: Svc.CmdSeqIn

        @ Command to wait for system quiescence before proceeding with start-up
        sync command WAIT_FOR_QUIESCENCE()

        @ Telemetry for boot count
        telemetry BootCount: FwSizeType update on change

        @ Whether the start-up manager is armed to wait for quiescence
        param ARMED: bool default true

        @ Time to wait before allowing start-up to proceed
        param QUIESCENCE_TIME: Fw.TimeIntervalValue default {seconds = 45 * 60, useconds = 0}

        @ File storing the quiescence start time
        param QUIESCENCE_START_FILE: string default "/quiescence_start.bin"

        @ Path to the start-up sequence file
        param STARTUP_SEQUENCE_FILE: string default "/startup.bin"

        @ File to store the boot count
        param BOOT_COUNT_FILE: string default "/boot_count.bin"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut
    }
}
