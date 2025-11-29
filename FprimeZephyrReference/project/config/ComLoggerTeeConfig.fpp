module ComLoggerTeeConfig {
    
    module QueueSizes {
        constant comLog = 10
    }
    
    module StackSizes {
        constant comLog = 8 * 1024  # Must match prj.conf thread stack size
    }

    module Priorities {
        constant comLog = 18
    }
}

