module Components {
    @ Component to manage system modes and orchestrate safe mode transitions
    @ based on voltage, watchdog faults, and communication timeouts
    active component ModeManager {

        # ----------------------------------------------------------------------
        # Input Ports
        # ----------------------------------------------------------------------

        @ Port receiving calls from the rate group (1Hz)
        sync input port run: Svc.Sched

        @ Port to receive watchdog fault signal on boot
        async input port watchdogFaultSignal: Fw.Signal

        @ Port to force safe mode entry (callable by other components)
        async input port forceSafeMode: Fw.Signal

        @ Port to clear all faults (callable by other components)
        async input port clearAllFaults: Fw.Signal

        # ----------------------------------------------------------------------
        # Output Ports
        # ----------------------------------------------------------------------

        @ Port to notify other components of mode changes
        output port modeChanged: Fw.Signal

        @ Ports to turn on LoadSwitch instances (8 total)
        output port loadSwitchTurnOn: [8] Fw.Signal

        @ Ports to turn off LoadSwitch instances (8 total)
        output port loadSwitchTurnOff: [8] Fw.Signal

        @ Port to set beacon divider parameter on ComDelay
        output port beaconDividerSet: Fw.PrmSet

        @ Port to get system voltage from INA219 manager
        output port voltageGet: Drv.VoltageGet

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Command to clear voltage fault flag
        @ Only succeeds if voltage > 7.5V
        sync command CLEAR_VOLTAGE_FAULT()

        @ Command to clear watchdog fault flag
        @ Can be cleared any time (manual only)
        sync command CLEAR_WATCHDOG_FAULT()

        @ Command to force system into safe mode
        sync command FORCE_SAFE_MODE()

        @ Command to set voltage thresholds
        sync command SET_VOLTAGE_THRESHOLDS(
            entryVoltage: F32 @< Voltage level to enter safe mode (default 7.0V)
            exitVoltage: F32 @< Voltage level required to exit safe mode (default 7.5V)
        )

        @ Command to manually exit safe mode
        @ Only succeeds if all faults are cleared
        @ Required after watchdog faults (prevents boot loops)
        sync command EXIT_SAFE_MODE()

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ Event emitted when entering safe mode
        event EnteringSafeMode(
            reason: string size 100 @< Reason for entering safe mode
        ) \
            severity warning high \
            format "ENTERING SAFE MODE: {}"

        @ Event emitted when exiting safe mode
        event ExitingSafeMode() \
            severity activity high \
            format "Exiting safe mode - all faults cleared"

        @ Event emitted when voltage fault is detected
        event VoltageFaultDetected(
            voltage: F32 @< Current voltage level
        ) \
            severity warning high \
            format "Voltage fault detected: {} V (threshold: 7.0V)"

        @ Event emitted when voltage fault is cleared
        event VoltageFaultCleared(
            voltage: F32 @< Current voltage level
        ) \
            severity activity high \
            format "Voltage fault cleared: {} V"

        @ Event emitted when watchdog fault is detected
        event WatchdogFaultDetected() \
            severity warning high \
            format "Watchdog fault detected - system was reset by watchdog timeout"

        @ Event emitted when watchdog fault is cleared
        event WatchdogFaultCleared() \
            severity activity high \
            format "Watchdog fault cleared by ground command"

        @ Event emitted when command validation fails
        event CommandValidationFailed(
            cmdName: string size 50 @< Command that failed validation
            reason: string size 100 @< Reason for failure
        ) \
            severity warning low \
            format "Command {} failed: {}"

        @ Event emitted when beacon backoff is updated
        event BeaconBackoffUpdated(
            divider: U32 @< New beacon divider value in seconds
        ) \
            severity activity low \
            format "Beacon backoff updated to {} seconds"

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Current system mode
        telemetry CurrentMode: U8

        @ Voltage fault flag status
        telemetry VoltageFaultFlag: bool

        @ Watchdog fault flag status
        telemetry WatchdogFaultFlag: bool

        @ Current system voltage
        telemetry CurrentVoltage: F32

        @ Current beacon backoff divider value
        telemetry BeaconBackoffDivider: U32

        @ Number of times safe mode has been entered
        telemetry SafeModeEntryCount: U32

        # ----------------------------------------------------------------------
        # Parameters
        # ----------------------------------------------------------------------

        @ Voltage threshold to enter safe mode (default: 7.0V)
        param VOLTAGE_ENTRY_THRESHOLD: F32 default 7.0

        @ Voltage threshold to allow clearing voltage fault (default: 7.5V)
        param VOLTAGE_EXIT_THRESHOLD: F32 default 7.5

        @ Maximum beacon backoff divider (default: 3600 seconds = 1 hour)
        param MAX_BEACON_BACKOFF: U32 default 3600

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

        @ Port for getting parameter values
        param get port prmGetOut

        @ Port for setting parameter values
        param set port prmSetOut

    }
}
