module Components {

    @ System mode enumeration
    enum SystemMode {
        SAFE_MODE = 1 @< Safe mode with non-critical components powered off
        NORMAL = 2 @< Normal operational mode
    }

    @ Reason for entering safe mode (used for recovery decision logic)
    enum SafeModeReason {
        NONE = 0 @< Not in safe mode or reason cleared
        LOW_BATTERY = 1 @< Entered due to low voltage condition
        SYSTEM_FAULT = 2 @< Entered due to unintended reboot/system fault
        GROUND_COMMAND = 3 @< Entered via ground command
        EXTERNAL_REQUEST = 4 @< Entered via external component request
        LORA = 5 @< Entered due to LoRa communication timeout or fault
    }

    @ Port for notifying about mode changes
    port SystemModeChanged(mode: SystemMode)

    @ Port for querying the current system mode
    port GetSystemMode -> SystemMode

    @ Port for forcing safe mode with a reason
    @ Pass NONE as reason to default to EXTERNAL_REQUEST
    port ForceSafeModeWithReason(reason: SafeModeReason)

    @ Component to manage system modes and orchestrate safe mode transitions
    @ based on voltage, watchdog faults, and communication timeouts
    active component ModeManager {

        # ----------------------------------------------------------------------
        # Input Ports
        # ----------------------------------------------------------------------

        @ Port receiving calls from the rate group (1Hz)
        sync input port run: Svc.Sched

        @ Port receiving completion status of the safe mode sequence
        sync input port completeSequence: Fw.CmdResponse

        @ Port to force safe mode entry (callable by other components)
        @ Accepts SafeModeReason - pass NONE to default to EXTERNAL_REQUEST
        async input port forceSafeMode: Components.ForceSafeModeWithReason

        @ Port to query the current system mode
        sync input port getMode: Components.GetSystemMode

        @ Port called before intentional reboot to set clean shutdown flag
        sync input port prepareForReboot: Fw.Signal

        # ----------------------------------------------------------------------
        # Output Ports
        # ----------------------------------------------------------------------

        @ Port to notify other components of mode changes (with current mode)
        output port modeChanged: [1] Components.SystemModeChanged

        @ Port for dispatching the safe mode command sequence
        output port runSequence: Svc.CmdSeqIn

        @ Ports to turn on LoadSwitch instances (6 face switches + 2 payload switches)
        output port loadSwitchTurnOn: [8] Fw.Signal

        @ Ports to turn off LoadSwitch instances (6 face switches + 2 payload switches)
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

        @ Event emitted when automatically entering safe mode due to low voltage
        event AutoSafeModeEntry(
            reason: SafeModeReason @< Reason for entering safe mode
            voltage: F32 @< Voltage that triggered the entry (0 if N/A)
        ) \
            severity warning high \
            format "AUTO SAFE MODE ENTRY: reason={} voltage={}V"

        @ Event emitted when automatically exiting safe mode due to voltage recovery
        event AutoSafeModeExit(
            voltage: F32 @< Voltage that triggered recovery
        ) \
            severity activity high \
            format "AUTO SAFE MODE EXIT: Voltage recovered to {}V"

        @ Event emitted when unintended reboot is detected
        event UnintendedRebootDetected() \
            severity warning high \
            format "UNINTENDED REBOOT DETECTED: Entering safe mode"

        @ Event emitted when preparing for intentional reboot
        event PreparingForReboot() \
            severity activity high \
            format "Preparing for intentional reboot - setting clean shutdown flag"


        @ this->log_ACTIVITY_HI_SafeModeSequenceCompleted();
        event SafeModeSequenceCompleted() \
            severity activity high \
            format "Safe Mode Radio Sequence Completed"

        @ this->log_WARNING_LO_SafeModeSequenceFailed(response);
        event SafeModeSequenceFailed(
            response: Fw.CmdResponse @< Response code
        ) \
            severity warning low \
            format "Safe Mode Radio Sequence Failed: {}"

        event SafeModeRequestIgnored() \
            severity warning low \
            format "SafeMode Request Ignored: Already in Safe Mode"

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Current system mode
        telemetry CurrentMode: U8

        @ Number of times safe mode has been entered
        telemetry SafeModeEntryCount: U32

        @ Current safe mode reason (NONE if not in safe mode)
        telemetry CurrentSafeModeReason: SafeModeReason

        # ----------------------------------------------------------------------
        # Parameters
        # ----------------------------------------------------------------------

        @ Voltage threshold for safe mode entry (V)
        param SafeModeEntryVoltage: F32 default 6.7

        @ Voltage threshold for safe mode recovery (V)
        param SafeModeRecoveryVoltage: F32 default 8.0

        @ Debounce time for voltage transitions (seconds)
        param SafeModeDebounceSeconds: U32 default 10

        param SAFEMODE_SEQUENCE_FILE: string default "/seq/radio_enter_safe.bin"

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
