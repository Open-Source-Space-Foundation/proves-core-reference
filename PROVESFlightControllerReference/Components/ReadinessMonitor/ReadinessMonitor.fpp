module Components {
    @ One bit per check in the PassMask, WarnMask, and FailMask channels.
    @ Append only: the bit positions are a contract with the ground readiness suite.
    enum ReadinessCheck : U8 {
        FS_ROOT = 0
        FS_SPACE = 1
        DOWNLINK = 2
        FACE_SENSORS = 3
        MTQ_DRIVERS = 4
        I2C_BUS = 5
        WDT_PET = 6
        RESET_CAUSE = 7
        BATTERY = 8
        FLIGHT_CONFIG = 9
        CLOCK_BOOT = 10
        RADIO = 11
    }

    enum CheckResult : U8 {
        NOT_RUN = 0
        PASS = 1
        WARN = 2
        FAIL = 3
    }

    @ Rolls up signals other components already produce into pass, warn, and fail masks.
    @ It only reports: it never commands, gates deploy or radio, or emits FATAL. Svc.Health stays the liveness path.
    active component ReadinessMonitor {
        constant NUM_TEMP_SENSORS = 9
        constant NUM_LIGHT_SENSORS = 5

        @ Cheap checks, once per call. Connect to the 1 Hz rate group: stall limits count calls as seconds.
        async input port run: Svc.Sched drop

        async input port pingIn: Svc.Ping drop

        output port pingOut: Svc.Ping

        @ Reads other components' latest channel values from TlmPacketizer
        output port tlmGet: Fw.TlmGet

        @ Reads other components' stored parameters from PrmDb
        output port prmDbGet: Fw.PrmGet

        @ Sampled only on a full run, never from run
        output port tempGet: [NUM_TEMP_SENSORS] Drv.temperatureGet

        @ Sampled only on a full run, never from run
        output port lightGet: [NUM_LIGHT_SENSORS] Drv.lightGet

        @ Full pass: adds the root listing, flight config, and face sensor reads to the cheap checks
        async command RUN_CHECKS() drop

        telemetry PassMask: U32 update on change
        telemetry WarnMask: U32 update on change
        telemetry FailMask: U32 update on change

        @ Bit per sensor slot that failed a powered read on the last full run (tempGet then lightGet order)
        telemetry SensorFailMask: U32 update on change

        event CheckDegraded(check: ReadinessCheck, was: CheckResult, now: CheckResult, detail: I32) \
            severity warning high \
            format "Readiness {}: {} -> {} (detail {})"

        event CheckCleared(check: ReadinessCheck, was: CheckResult, now: CheckResult, detail: I32) \
            severity activity high \
            format "Readiness {}: {} -> {} (detail {})"

        event ReadinessSummary(pass: U8, warn: U8, fail: U8) \
            severity activity high \
            format "Readiness: {} pass, {} warn, {} fail"

        param FS_FREE_FLOOR_BYTES: FwSizeType default 262144

        @ A file buffer held in the LoRa queue this long means the downlink is not making progress
        param DOWNLINK_STALL_S: U32 default 120

        @ After quiescence ends, LoRa bytes sent must rise within this window
        param RADIO_SILENT_S: U32 default 600

        @ WatchdogTransitions must rise within this window
        param WDT_STALL_S: U32 default 5

        param VBATT_WARN_V: F64 default 7.0

        @ Seconds after boot before the first automatic full run. 0 disables it.
        param BOOT_FULL_RUN_DELAY_S: U32 default 120

        time get port timeCaller
        command reg port cmdRegOut
        command recv port cmdIn
        command resp port cmdResponseOut
        text event port logTextOut
        event port logOut
        telemetry port tlmOut
        param get port prmGetOut
        param set port prmSetOut
    }
}
