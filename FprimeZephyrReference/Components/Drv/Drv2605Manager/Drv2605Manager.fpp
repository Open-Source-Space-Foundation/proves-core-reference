module Drv {
    port triggerDevice -> bool
}

module Drv {
    @ Component for F Prime FSW framework.
    passive component Drv2605Manager {

        @ Port to initialize the DRV2605 device
        sync input port init: Fw.SuccessCondition

        @ Port to trigger the DRV2605 device
        sync input port triggerDevice: triggerDevice

        @ Event for reporting DRV2605 not ready error
        # event DeviceNotReady() severity warning high format "DRV2605 device not ready"
        event DeviceNotReady() severity warning high format "DRV2605 device not ready" throttle 5

        @ Event for reporting DRV2605 initialization failure
        event DeviceInitFailed(ret: I32) severity warning high format "DRV2605 initialization failed with return code: {}" throttle 5

        @ Event for reporting DRV2605 nil device error
        event DeviceNil() severity warning high format "DRV2605 device is nil" throttle 5

        @ Event for reporting DRV2605 nil state error
        event DeviceStateNil() severity warning high format "DRV2605 device state is nil" throttle 5

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

    }
}
