module Components {
    @ Component for F Prime FSW framework.
    enum Status {
        ENABLED
        DISABLED
    }

    passive component TlmArchive {
        sync command RECORDING_STATUS(status: Status)

        telemetry RecordingEnabled: Fw.On

        event Debug(message: string) severity activity low format "{}"

        sync input port run: Svc.Sched
        sync input port comIn: Fw.Com

        @ Port for checking whether antenna deployment has completed
        output port deploymentStateGet: Components.GetDeploymentState

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}
