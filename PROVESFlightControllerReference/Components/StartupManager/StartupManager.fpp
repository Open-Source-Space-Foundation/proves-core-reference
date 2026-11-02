module Components {
    @ Manages the start-up sequencing
    passive component StartupManager {
        @ Check RTC time diff
        sync input port run: Svc.Sched

        @ Port for sending sequence dispatches
        output port runSequence: Svc.CmdSeqIn

        @ Port for receiving the status of the start-up sequence
        sync input port startupCompleteSequence: Fw.CmdResponse

        @ Port for receiving the indication that the start-up sequence has started
        sync input port startupsequenceStarted: Svc.CmdSeqIn

        @ Port for receiving the status of the SafeMode Sequence
        sync input port safeModeCompleteSequence: Fw.CmdResponse

        @ Port for receiving the indication that the SafeMode sequence has started
        sync input port safeModeSequenceStarted: Svc.CmdSeqIn

        @ Port for receiving the indication that the Payload sequence has started
        sync input port payloadSequenceStarted: Svc.CmdSeqIn

        @ Port for receiving the status of the Payload Sequence
        sync input port payloadCompleteSequence: Fw.CmdResponse

        @ Port for receiving if Lora was ever on
        sync input port loraFirstStart: Fw.Signal

        @ Enable LoRa transmission
        output port enableTransmit: Fw.Signal

        @ Disable LoRa transmission
        output port disableTransmit: Fw.Signal

        @ Command to output the current boot count
        sync command GET_BOOT_COUNT()

        @ Telemetry for boot count
        telemetry BootCount: FwSizeType

        @ Event emitted when getting boot count
        event CurrentBootCount(i: I64) severity activity low \
            format "Current boot count: {}"

        @ Event emitted when failing to update the boot count file
        event BootCountUpdateFailure() severity warning low \
            format "Failed to update boot count file"

        @ Event emitted when the boot count file holds an implausible value (torn or corrupt write)
        event BootCountCorrupted(raw: I64) severity warning high \
            format "Boot count file corrupt (raw value {}) - treating as unreadable"

        @ Event emitted when the start-up sequence succeeds
        event StartupSequenceFinished() severity activity low \
            format "Start-up sequence finished successfully"

        @ Event emitted when the start-up sequence fails
        event StartupSequenceFailed(response: Fw.CmdResponse @< Response code
            ) severity warning low format "Start-up sequence failed with response code {}"

        @ Event emitted when the hardcoded startup sequence enables the radio
        event HardcodedRadioEnable() severity activity high \
            format "Hardcoded Startup Sequence has enabled the radio"

        @ Path to the start-up sequence file
        param STARTUP_SEQUENCE_FILE: string default "/seq/startup.bin"

        @ File to store the boot count
        param BOOT_COUNT_FILE: string default "/boot_count.bin"

        @ 1 Hz run ticks before enabling LoRa transmit (45*60 + 100 margin)
        param TRANSMIT_ENABLE_TICKS: U32 default 2800

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

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

    }
}
