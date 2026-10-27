module Drv {
    @ Recovers an I2C bus that a target holds low. See docs/sdd.md.
    passive component I2cBusMonitor {

        #### Ports ####
        @ 1 Hz poll: recover the bus if a line reads low
        sync input port run: Svc.Sched

        #### Commands ####
        @ Recover the bus now, even if both lines read high
        sync command RECOVER_BUS()

        @ Choose which recovery steps run (bits: 1 halt controller, 2 clock out while SDA low, 4 always clock 9 times, 8 mux reset). Default 11. Not saved across a reboot
        sync command SET_RECOVERY_STEPS(steps: U8)

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

        @ The I2C driver ran the recovery after transfers timed out since the last poll
        event TimeoutRecoveries(recovered: U32, failed: U32) severity warning high format "I2C transfer timeouts: {} recoveries released the bus, {} left a line low" throttle 5

        @ State recorded at one recovery. source: 0 driver timeout, 1 poll, 2 command. lines: SDA bit 0 and SCL bit 1 before the sequence, then after halt, clock out and mux reset, 2 bits each
        event RecoveryDetail(uptimeMs: U32, source: U8, tar: U16, muxChannel: U8, status: U32, rawIntr: U32, abortSource: U32, txflr: U8, rxflr: U8, lines: U8, clocks: U8, steps: U8, ret: I32) severity warning low format "I2C recovery at {} ms (source {}): TAR=0x{x} mux=0x{x} IC_STATUS=0x{x} RAW_INTR=0x{x} ABRT=0x{x} TXFLR={} RXFLR={} lines=0x{x} clocks={} steps=0x{x} ret={}"

        @ The detail ring was full, so some recoveries were not recorded
        event RecoveryDetailsDropped(count: U32) severity warning low format "{} I2C recovery details dropped" throttle 5

        @ SET_RECOVERY_STEPS changed the recovery steps
        event RecoveryStepsSet(steps: U8) severity activity high format "I2C recovery steps set to 0x{x}"

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
