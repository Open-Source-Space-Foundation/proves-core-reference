module Drv {
    port StartMagnetorquer(
        val: I8, @< Magnetorquer drive level as a signed current scale in the range -127 to 127.
    ) -> Fw.Success
    port StopMagnetorquer() -> Fw.Success
    port MagnetorquerToggle(value: bool)
}

module Drv {
    @ Manager for the DRV2605 device
    passive component Drv2605Manager {

        #### Ports ####
        @ Port to start the magnetorquer
        sync input port start: StartMagnetorquer

        @ Port to stop the magnetorquer
        sync input port stop: StopMagnetorquer

        @ Port to initialize and deinitialize the device on load switch state change
        sync input port loadSwitchStateChanged: Components.loadSwitchStateChanged

        #### Commands ####
        @ Command to start the magnetorquer
        sync command START(val: I8)

        @ Command to stop the magnetorquer
        sync command STOP()

        #### Events ####
        @ Event for reporting not ready error
        event DeviceNotReady() severity warning low format "Device not ready" throttle 5

        @ Event for reporting initialization failure
        event DeviceInitFailed(ret: I32) severity warning low format "Initialization failed with return code: {}" throttle 5

        @ Event for reporting nil device error
        event DeviceNil() severity warning low format "Device is nil" throttle 5

        @ Event for reporting nil state error
        event DeviceStateNil() severity warning low format "Device state is nil" throttle 5

        @ Event for reporting error setting hatpic config
        event DeviceHapticConfigSetFailed(ret: I32) severity warning low format "Setting haptic config failed with return code: {}" throttle 5

        @ Event for reporting TCA unhealthy state
        event TcaUnhealthy() severity warning low format "TCA device is unhealthy" throttle 5

        @ Event for reporting MUX unhealthy state
        event MuxUnhealthy() severity warning low format "MUX device is unhealthy" throttle 5

        @ Event for reporting failure to trigger the magnetorquer
        event TriggerFailed(ret: I32) severity warning low format "Trigger failed with return code: {}" throttle 5

        @ Event for reporting when the DRV2605 has successfully initialized
        event DeviceInitialized() severity activity low format "DRV2605 device initialized."

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

    }
}
