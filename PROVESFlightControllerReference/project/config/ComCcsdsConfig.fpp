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
        # issue #471: depth 1 silently dropped any telemetry packet that
        # arrived while another was queued (QueueOverflow at index 1), which
        # is why buffer-pool health channels never reached the ground.
        constant tlm         = 8
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
        # issue #471: must exceed FileHandling fileUplink queue size (10) plus
        # in-pipeline slack, or a stalled SD write exhausts the pool mid-uplink
        # and the AllocationError FATALs the board (HiBuffs measured at 10/10
        # during a single 204KB uplink with the old count of 5).
        constant commsFileBuffCount    = 20
        constant commsBuffMgrId        = 200
    }
}
