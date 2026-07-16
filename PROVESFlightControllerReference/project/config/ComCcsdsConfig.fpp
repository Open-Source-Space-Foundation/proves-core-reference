module ComCcsdsConfig {
    #Base ID for the ComCcsds Subtopology, all components are offsets from this base ID
    constant BASE_ID = 0x02000000
    constant BASE_ID_UART = 0x21000000
    constant BASE_ID_LORA = 0x22000000
    constant BASE_ID_SBAND = 0x23000000

    module QueueSizes {
        constant comQueue            = 20
        constant aggregator          = 15
    }

    module StackSizes {
        constant comQueue            = 4 * 1024 # Must match prj.conf thread stack size
        constant aggregator          = 4 * 1024 # Must match prj.conf thread stack size
    }

    module Priorities {
        constant aggregator = 7 # Aggregator (consumer) must have higher priority than comQueue (producer)
        constant comQueue   = 8 # ComQueue has higher priority than data producers (e.g. events, telemetry)
    }

    # Queue configuration constants
    module QueueDepths {
        constant events      = 50
        constant tlm         = 1
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
        constant commsFileBuffSize     = 1024
        constant commsBuffCount        = 5
        constant commsFileBuffCount    = 5
        constant commsBuffMgrId        = 200
    }

    # Lean profile for the S-Band subtopology. Three full-size ComCcsds stacks do
    # not fit the RP2350 malloc arena (~149 KB): each stack costs ~35.6 KB of heap
    # (comQueue table ~13.5K + buffer pool ~11.2K + frame accumulator ~1K + the two
    # OS message queues ~9.9K), and the third one exhausted the arena during
    # configComponents (HWIL 2026-07-16). S-Band is a secondary downlink; it gets
    # smaller queues and a smaller buffer pool (~17 KB total instead of ~35.6 KB).
    module Sband {
        module QueueSizes {
            constant comQueue            = 10
            constant aggregator          = 8
        }
        module QueueDepths {
            constant events      = 15
            constant tlm         = 1
            constant file        = 1
        }
        module BuffMgr {
            constant commsBuffCount     = 3
            constant commsFileBuffCount = 3
        }
    }
}
