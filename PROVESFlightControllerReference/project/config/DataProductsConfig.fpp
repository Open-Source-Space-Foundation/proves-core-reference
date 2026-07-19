module DataProductsConfig {
    #Base ID for the DataProducts Subtopology, all components are offsets from this base ID
    constant BASE_ID = 0x04000000

    module QueueSizes {
        constant dpCat           = 10
        constant dpMgr           = 40
        constant dpWriter        = 40
        constant dpBufferManager = 10
    }

    # Zephyr dynamic thread stacks are 4 KB; the stock 64 KB stacks do not fit
    module StackSizes {
        constant dpCat           = 8 * 1024
        constant dpMgr           = 8 * 1024
        constant dpWriter        = 8 * 1024
        constant dpBufferManager = 8 * 1024
    }

    module Priorities {
        constant dpMgr           = 13
        constant dpWriter        = 14
        constant dpBufferManager = 14
        constant dpCat           = 15
    }

    # Buffer management constants
    # Data product containers: 100 samples * 8 bytes + ~125 bytes DP packet header/hash overhead
    module BuffMgr {
        constant dpBufferStoreSize  = 1024
        constant dpBufferStoreCount = 2
        constant dpBufferManagerId  = 4
    }

    # Directory and file paths
    module Paths {
        constant dpDir    = "/dp"
        constant dpState  = "/dp/DpState.dat"
    }
}
