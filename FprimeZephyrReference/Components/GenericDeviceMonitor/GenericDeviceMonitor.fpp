module Components {
    port HealthGet -> Fw.Health
}

module Components {
    @ Generic Device Health Reporter
    @ Use for devices which only need to report simple health status and have no interactivity
    passive component GenericDeviceMonitor {

        @ Port receiving calls from the rate group
        sync input port run: Svc.Sched

        @ Port receiving calls to request device health
        sync input port healthGet: HealthGet

        @ Telemetry showing device health
        telemetry Healthy: Fw.Health

        @ Event indicating device not ready
        event DeviceNotReady() severity warning low id 0 format "Device not ready" throttle 5

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

    }
}
