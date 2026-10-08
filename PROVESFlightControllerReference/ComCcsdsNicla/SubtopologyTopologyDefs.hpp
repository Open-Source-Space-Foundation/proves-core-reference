#ifndef COMCCSDSNICLASUBTOPOLOGY_DEFS_HPP
#define COMCCSDSNICLASUBTOPOLOGY_DEFS_HPP

#include <Fw/Types/MallocAllocator.hpp>
#include <Svc/BufferManager/BufferManager.hpp>
#include <Svc/FrameAccumulator/FrameDetector/CcsdsTcFrameDetector.hpp>

#include "ComCcsdsConfig/ComCcsdsSubtopologyConfig.hpp"
#include "Svc/Subtopologies/ComCcsds/ComCcsdsConfig/FppConstantsAc.hpp"

namespace ComCcsdsNicla {
struct SubtopologyState {
    // Empty - no external state needed for ComCcsdsNicla subtopology
};

struct TopologyState {
    SubtopologyState comCcsdsNicla;
};
}  // namespace ComCcsdsNicla

#endif
