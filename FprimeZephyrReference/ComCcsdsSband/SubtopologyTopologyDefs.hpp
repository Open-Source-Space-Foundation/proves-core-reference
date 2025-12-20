#ifndef COMCCSDSSBANDSUBDEFS_HPP
#define COMCCSDSSBANDSUBDEFS_HPP

#include <Fw/Types/MallocAllocator.hpp>
#include <Svc/BufferManager/BufferManager.hpp>
#include <Svc/FrameAccumulator/FrameDetector/CcsdsTcFrameDetector.hpp>

#include "ComCcsdsConfig/ComCcsdsSubtopologyConfig.hpp"
#include "Svc/Subtopologies/ComCcsds/ComCcsdsConfig/FppConstantsAc.hpp"

namespace ComCcsdsSband {
struct SubtopologyState {
    // Empty - no external state needed for ComCcsdsSband subtopology
};

struct TopologyState {
    SubtopologyState comCcsdsSband;
};
}  // namespace ComCcsdsSband

#endif
