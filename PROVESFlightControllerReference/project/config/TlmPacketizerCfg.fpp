# ======================================================================
# TlmPacketizerCfg.fpp
# Project-specific TlmPacketizer configuration
# Groups 1-6 are used in ReferenceDeploymentPackets.fppi
# ======================================================================
module Svc {
    @ One output section: REALTIME downlink through communication stack
    enum TelemetrySection {
        REALTIME,         @< Realtime telemetry downlink through communication stack
        NUM_SECTIONS,     @< REQUIRED: Counter, leave as last element.
    }

    @ Max group used in ReferenceDeploymentPackets.fppi is 6
    constant MAX_CONFIGURABLE_TLMPACKETIZER_GROUP = 6

    constant NUM_CONFIGURABLE_TLMPACKETIZER_GROUPS = MAX_CONFIGURABLE_TLMPACKETIZER_GROUP + 1

    @ One output port — PktSend[0] is connected to comSplitterTelemetry
    constant TELEMETRY_SEND_PORTS = TelemetrySection.NUM_SECTIONS

    @ All sections/groups send on port 0
    constant TELEMETRY_SEND_PORT_MAPPING = [
        [0, 0, 0, 0, 0, 0, 0],
    ]

    @ All sections start ENABLED
    constant TELEMETRY_SECTION_ENABLED_DEFAULTS = [Fw.Enabled.ENABLED]

    @ Default group config: output on change, no min/max time thresholds
    constant DEFAULT_GROUP_CONFIG = { enabled = Fw.Enabled.ENABLED, forceEnabled = Fw.Enabled.DISABLED, rateLogic = RateLogic.ON_CHANGE_MIN, min = 0, max = 0 }

    @ Default configuration for all sections and groups
    constant TELEMETRY_SECTION_DEFAULTS = [
        # REALTIME section
        [
            DEFAULT_GROUP_CONFIG,  # Group 0
            DEFAULT_GROUP_CONFIG,  # Group 1
            DEFAULT_GROUP_CONFIG,  # Group 2
            DEFAULT_GROUP_CONFIG,  # Group 3
            DEFAULT_GROUP_CONFIG,  # Group 4
            DEFAULT_GROUP_CONFIG,  # Group 5
            DEFAULT_GROUP_CONFIG,  # Group 6
        ],
    ]
}
