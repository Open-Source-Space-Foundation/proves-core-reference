#ifndef COMCCSDSSUBTOPOLOGY_DEFS_HPP
#define COMCCSDSSUBTOPOLOGY_DEFS_HPP

#include <Fw/Types/MallocAllocator.hpp>
#include <Svc/BufferManager/BufferManager.hpp>
#include <Svc/FrameAccumulator/FrameDetector/CcsdsTcFrameDetector.hpp>

#include "ComCcsdsConfig/ComCcsdsSubtopologyConfig.hpp"
#include "Svc/Subtopologies/ComCcsds/ComCcsdsConfig/FppConstantsAc.hpp"

namespace ComCcsdsUart {
struct SubtopologyState {
    // Empty - no external state needed for ComCcsdsUart subtopology
};

struct TopologyState {
    SubtopologyState comCcsdsUart;
};
}  // namespace ComCcsdsUart

#endif
