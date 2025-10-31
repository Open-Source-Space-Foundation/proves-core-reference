module ComCcsdsConfig {
    #Base ID for the ComCcsds Subtopology, all components are offsets from this base ID
    constant BASE_ID = 0x02000000
    constant BASE_ID_UART = 0x21000000

    module QueueSizes {
        constant comQueue   = 5
        constant aggregator = 15
    }

    module StackSizes {
        constant comQueue   = 8 * 1024 # Must match prj.conf thread stack size
        constant aggregator = 8 * 1024 # Must match prj.conf thread stack size
    }

    module Priorities {
        constant aggregator = 5 # Aggregator (consumer) must have higher priority than comQueue (producer)
        constant comQueue   = 6 # ComQueue has higher priority than data producers (e.g. events, telemetry)
    }

    # Queue configuration constants
    module QueueDepths {
        constant events      = 10
        constant tlm         = 5
        constant file        = 1
    }

    module QueuePriorities {
        constant events      = 0
        constant tlm         = 2
        constant file        = 1
    }

    # Buffer management constants
    module BuffMgr {
        constant frameAccumulatorSize  = 1024 # Must be at least as large as the comm buffer size
        constant commsBuffSize         = 1024 # Size of ring buffer
        constant commsFileBuffSize     = 1
        constant commsBuffCount        = 5
        constant commsFileBuffCount    = 1
        constant commsBuffMgrId        = 200
    }
}
