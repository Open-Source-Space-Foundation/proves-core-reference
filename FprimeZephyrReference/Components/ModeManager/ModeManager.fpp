module Components {
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


        # ----------------------------------------------------------------------
        # Output Ports
        # ----------------------------------------------------------------------

        @ Port to notify other components of mode changes
        output port modeChanged: Fw.Signal

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

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Current system mode
        telemetry CurrentMode: U8


        @ Current system voltage
        telemetry CurrentVoltage: F32

        @ Number of times safe mode has been entered
        telemetry SafeModeEntryCount: U32


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
