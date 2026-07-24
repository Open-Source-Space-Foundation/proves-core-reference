module FileHandlingConfig {
    #Base ID for the FileHandling Subtopology, all components are offsets from this base ID
    constant BASE_ID = 0x05000000

    module QueueSizes {
        # issue #471: must hold the entire comms buffer pool (25 = commsBuffCount
        # + commsFileBuffCount in ComCcsdsConfig); an SD stall queues every
        # in-flight buffer here and queue-full is an FW_ASSERT (FATAL).
        constant fileUplink    = 30
        constant fileDownlink  = 10
        constant fileManager   = 10
        constant prmDb         = 10
    }

    module StackSizes {
        constant fileUplink    = 4 * 1024
        constant fileDownlink  = 4 * 1024
        constant fileManager   = 4 * 1024
        constant prmDb         = 4 * 1024
    }

    module Priorities {
        constant fileUplink    = 9
        constant fileDownlink  = 9
        constant prmDb         = 10
        constant fileManager   = 15
    }

    # File downlink configuration constants
    module DownlinkConfig {
        constant timeout        = 5000         # File downlink timeout in ms
        constant cooldown       = 1000         # File downlink cooldown in ms
        constant cycleTime      = 1000         # File downlink cycle time in ms
        constant fileQueueDepth = 3           # File downlink queue depth
    }
}
