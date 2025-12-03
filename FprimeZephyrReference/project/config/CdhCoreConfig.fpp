module CdhCoreConfig {
    #Base ID for the CdhCore Subtopology, all components are offsets from this base ID
    constant BASE_ID = 0x01000000

    module QueueSizes {
        constant cmdDisp     = 10
        constant events      = 25
        constant tlmSend     = 5
        constant $health     = 20
    }


    module StackSizes {
        constant cmdDisp     = 4 * 1024 # Must match prj.conf thread stack size
        constant events      = 4 * 1024 # Must match prj.conf thread stack size
        constant tlmSend     = 4 * 1024 # Must match prj.conf thread stack size
    }

    module Priorities {
        constant cmdDisp     = 7
        constant $health     = 8
        constant events      = 9
        constant tlmSend     = 10

    }
}
