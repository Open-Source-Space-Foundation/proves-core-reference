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
        constant cmdDisp     = 4 * 1024 # Must match CONFIG_DYNAMIC_THREAD_STACK_SIZE in prj.conf
        constant events      = 4 * 1024 # Must match CONFIG_DYNAMIC_THREAD_STACK_SIZE in prj.conf
        constant tlmSend     = 4 * 1024 # Must match CONFIG_DYNAMIC_THREAD_STACK_SIZE in prj.conf
    }

    module Priorities {
        constant cmdDisp     = 4
        constant $health     = 5
        constant events      = 6
        constant tlmSend     = 6

    }
}
