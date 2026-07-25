module Components {
    @ Stores telemetry generated before antenna deployment
    passive component TlmArchive {
        @ Telemetry packet to archive
        sync input port comIn: Fw.Com

        @ Port for checking whether antenna deployment has completed
        output port deploymentStateGet: Components.GetDeploymentState
    }
}
