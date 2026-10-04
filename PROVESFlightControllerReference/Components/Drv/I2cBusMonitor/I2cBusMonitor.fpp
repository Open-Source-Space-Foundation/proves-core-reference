module Drv {
    @ Watches an I2C bus for a target holding SDA/SCL low and recovers it by
    @ resetting the TCA954x mux and clocking SCL (9 clocks + STOP)
    passive component I2cBusMonitor {

        #### Ports ####
        @ Rate group tick: sample SDA/SCL and recover the bus if a line is stuck low
        sync input port run: Svc.Sched

        #### Commands ####
        @ Force a bus recovery (mux reset + 9 SCL clocks + STOP) even if the lines read idle
        sync command RECOVER_BUS()

        #### Telemetry ####
        @ Number of recoveries that found a stuck bus and released it
        telemetry RecoveryCount: U32

        @ Number of recovery attempts that left a line stuck low
        telemetry RecoveryFailureCount: U32

        #### Events ####
        @ A line was low while no transfer was in flight
        event BusStuck(sda: U8, scl: U8) severity warning high format "I2C bus stuck (SDA={}, SCL={}), recovering" throttle 5

        @ The bus was recovered
        event BusRecovered(sda: U8, scl: U8) severity activity high format "I2C bus recovered (SDA={}, SCL={})"

        @ The bus is still stuck after recovery
        event BusRecoveryFailed(ret: I32, sda: U8, scl: U8) severity warning high format "I2C bus recovery failed with return code {} (SDA={}, SCL={})" throttle 5

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
