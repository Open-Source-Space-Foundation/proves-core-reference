module Drv {
    @ Recovers an I2C bus that a target holds low. See docs/sdd.md.
    passive component I2cBusMonitor {

        #### Ports ####
        @ 1 Hz poll: recover the bus if a line reads low
        sync input port run: Svc.Sched

        #### Commands ####
        @ Recover the bus even if both lines read high
        sync command RECOVER_BUS()

        #### Telemetry ####
        @ Recoveries that left both lines high
        telemetry RecoveryCount: U32

        @ Recoveries that left a line low
        telemetry RecoveryFailureCount: U32

        #### Events ####
        @ A line was low with no transfer in progress
        event BusStuck(sda: U8, scl: U8) severity warning high format "I2C bus stuck (SDA={}, SCL={}), recovering" throttle 5

        @ Recovery left both lines high
        event BusRecovered() severity activity high format "I2C bus recovered" throttle 5

        @ Recovery left a line low or returned an error
        event BusRecoveryFailed(ret: I32, sda: U8, scl: U8) severity warning high format "I2C bus recovery failed with return code {} (SDA={}, SCL={})" throttle 5

        @ The 1 Hz poll limits the rate of automatic recovery
        event AutoRecoveryBackOff(recoveries: U32, interval: U32) severity warning high format "{} I2C bus recoveries without a clean bus, auto recovery limited to one every {} s"

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
