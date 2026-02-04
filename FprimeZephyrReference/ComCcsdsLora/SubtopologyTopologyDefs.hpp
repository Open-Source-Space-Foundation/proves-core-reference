#ifndef COMCCSDSLORASUBTOPOLOGY_DEFS_HPP
#define COMCCSDSLORASUBTOPOLOGY_DEFS_HPP

#include <Fw/Types/MallocAllocator.hpp>
#include <Svc/BufferManager/BufferManager.hpp>
#include <Svc/FrameAccumulator/FrameDetector/CcsdsTcFrameDetector.hpp>

#include "ComCcsdsConfig/ComCcsdsSubtopologyConfig.hpp"
#include "Svc/Subtopologies/ComCcsds/ComCcsdsConfig/FppConstantsAc.hpp"

namespace ComCcsdsLora {
struct SubtopologyState {
    // Empty - no external state needed for ComCcsdsLora subtopology
};

struct TopologyState {
    SubtopologyState comCcsdsLora;
};
}  // namespace ComCcsdsLora

#endif
