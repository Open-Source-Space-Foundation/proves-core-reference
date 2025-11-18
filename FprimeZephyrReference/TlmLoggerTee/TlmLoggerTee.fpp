module TlmLoggerTee {
    constant BASE_ID = 0x10A00000
    
    # Include the subtopology template from the specified path
    include "../../lib/fprime/Svc/Subtopologies/ComLoggerTee/subtopology-template.fppi"
}