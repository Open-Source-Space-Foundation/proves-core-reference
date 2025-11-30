module Components {

    @ System mode enumeration
    enum SystemMode {
        HIBERNATION_MODE = 0 @< Ultra-low-power hibernation with periodic wake windows
        SAFE_MODE = 1 @< Safe mode with non-critical components powered off
        NORMAL = 2 @< Normal operational mode
        PAYLOAD_MODE = 3 @< Payload mode with payload power and battery enabled
    }

    @ Port for notifying about mode changes
    port SystemModeChanged(mode: SystemMode)

    @ Port for querying the current system mode
    port GetSystemMode -> SystemMode

    @ Component to manage system modes and orchestrate safe mode transitions
    @ based on voltage, watchdog faults, and communication timeouts
    active component ModeManager {

        # ----------------------------------------------------------------------
        # Input Ports
        # ----------------------------------------------------------------------

        @ Port receiving calls from the rate group (1Hz)
        sync input port run: Svc.Sched

        @ Port to force safe mode entry (callable by other components)
        async input port forceSafeMode: Fw.Signal

        @ Port to query the current system mode
        sync input port getMode: Components.GetSystemMode

        # ----------------------------------------------------------------------
        # Output Ports
        # ----------------------------------------------------------------------

        @ Port to notify other components of mode changes (with current mode)
        output port modeChanged: Components.SystemModeChanged

        @ Ports to turn on LoadSwitch instances (8 total)
        output port loadSwitchTurnOn: [8] Fw.Signal

        @ Ports to turn off LoadSwitch instances (8 total)
        output port loadSwitchTurnOff: [8] Fw.Signal

        @ Port to get system voltage from INA219 manager
        output port voltageGet: Drv.VoltageGet

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------


        @ Command to force system into safe mode
        sync command FORCE_SAFE_MODE()

        @ Command to manually exit safe mode
        @ Only succeeds if currently in safe mode
        sync command EXIT_SAFE_MODE()

        @ Command to enter payload mode
        @ Only succeeds if currently in normal mode
        sync command ENTER_PAYLOAD_MODE()

        @ Command to exit payload mode
        @ Only succeeds if currently in payload mode
        sync command EXIT_PAYLOAD_MODE()

        @ Command to enter hibernation mode (only from SAFE_MODE)
        @ Uses RP2350 dormant mode with RTC alarm wake
        @ sleepDurationSec: Duration of each sleep cycle in seconds (default 3600 = 60 min)
        @ wakeDurationSec: Duration of each wake window in seconds (default 60 = 1 min)
        sync command ENTER_HIBERNATION(
            sleepDurationSec: U32 @< Sleep cycle duration in seconds (0 = default 3600)
            wakeDurationSec: U32 @< Wake window duration in seconds (0 = default 60)
        )

        @ Command to exit hibernation mode
        @ Only succeeds if currently in hibernation mode wake window
        @ Transitions to SAFE_MODE
        sync command EXIT_HIBERNATION()

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
            format "Exiting safe mode"

        @ Event emitted when safe mode is manually commanded
        event ManualSafeModeEntry() \
            severity activity high \
            format "Safe mode entry commanded manually"

        @ Event emitted when external fault is detected
        event ExternalFaultDetected() \
            severity warning high \
            format "External fault detected - external component forced safe mode"

        @ Event emitted when entering payload mode
        event EnteringPayloadMode(
            reason: string size 100 @< Reason for entering payload mode
        ) \
            severity activity high \
            format "ENTERING PAYLOAD MODE: {}"

        @ Event emitted when exiting payload mode
        event ExitingPayloadMode() \
            severity activity high \
            format "Exiting payload mode"

        @ Event emitted when payload mode is manually commanded
        event ManualPayloadModeEntry() \
            severity activity high \
            format "Payload mode entry commanded manually"

        @ Event emitted when command validation fails
        event CommandValidationFailed(
            cmdName: string size 50 @< Command that failed validation
            reason: string size 100 @< Reason for failure
        ) \
            severity warning low \
            format "Command {} failed: {}"

        @ Event emitted when state persistence fails
        event StatePersistenceFailure(
            operation: string size 20 @< Operation that failed (save/load)
            status: I32 @< File operation status code
        ) \
            severity warning low \
            format "State persistence {} failed with status {}"

        @ Event emitted when entering hibernation mode
        event EnteringHibernation(
            reason: string size 100 @< Reason for entering hibernation
            sleepDurationSec: U32 @< Sleep cycle duration in seconds
            wakeDurationSec: U32 @< Wake window duration in seconds
        ) \
            severity warning high \
            format "ENTERING HIBERNATION: {} (sleep={}s, wake={}s)"

        @ Event emitted when exiting hibernation mode
        event ExitingHibernation(
            cycleCount: U32 @< Total hibernation cycles completed
            totalSeconds: U32 @< Total time spent in hibernation
        ) \
            severity activity high \
            format "Exiting hibernation after {} cycles ({}s total)"

        @ Event emitted when hibernation wake window starts
        event HibernationWakeWindow(
            cycleNumber: U32 @< Current wake cycle number
        ) \
            severity activity low \
            format "Hibernation wake window #{}"

        @ Event emitted when starting a new hibernation sleep cycle
        event HibernationSleepCycle(
            cycleNumber: U32 @< Sleep cycle number starting
        ) \
            severity activity low \
            format "Hibernation sleep cycle #{} starting"

        @ Event emitted when hibernation dormant sleep entry fails
        @ CRITICAL: Ground already received OK response before this failure
        @ Mode reverts to SAFE_MODE, counters are rolled back
        event HibernationEntryFailed(
            reason: string size 100 @< Reason for failure
        ) \
            severity warning high \
            format "HIBERNATION ENTRY FAILED (command ack'd OK but dormant failed): {}"

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Current system mode
        telemetry CurrentMode: U8

        @ Number of times safe mode has been entered
        telemetry SafeModeEntryCount: U32

        @ Number of times payload mode has been entered
        telemetry PayloadModeEntryCount: U32

        @ Number of hibernation sleep/wake cycles completed
        telemetry HibernationCycleCount: U32

        @ Total time spent in hibernation (seconds)
        telemetry HibernationTotalSeconds: U32


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
