# TlmArchive

`TlmArchive` appends telemetry packets to `//tlm/pre_deployment.tlm`
while the antenna deployment state is false. Once the antenna is marked
deployed, incoming telemetry is no longer written.
