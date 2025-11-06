# ======================================================================
# AcConstants.fpp
# F Prime configuration constants (project override)
# ======================================================================

# Override: Increase from default 10 to accommodate all 1Hz rate group members
constant ActiveRateGroupOutputPorts = 15

# Defaults
constant PassiveRateGroupOutputPorts = 10
constant RateGroupDriverRateGroupPorts = 3
constant CmdDispatcherComponentCommandPorts = 30
constant CmdDispatcherSequencePorts = 5
constant SeqDispatcherSequencerPorts = 2
constant CmdSplitterPorts = CmdDispatcherSequencePorts
constant StaticMemoryAllocations = 4
constant HealthPingPorts = 25
constant FileDownCompletePorts = 1
constant ComQueueComPorts = 2
constant ComQueueBufferPorts = 1
constant BufferRepeaterOutputPorts = 10
constant DpManagerNumPorts = 5
constant DpWriterNumProcPorts = 5
constant FileNameStringSize = 200
constant FwAssertTextSize = 120
constant AssertFatalAdapterEventFileSize = FileNameStringSize

# Hub connections
constant GenericHubInputPorts = 10
constant GenericHubOutputPorts = 10
constant GenericHubInputBuffers = 10
constant GenericHubOutputBuffers = 10
