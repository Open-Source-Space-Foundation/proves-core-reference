#ifndef COMCCSDSSUBTOPOLOGY_CONFIG_HPP
#define COMCCSDSSUBTOPOLOGY_CONFIG_HPP

#include "Fw/Types/MallocAllocator.hpp"

namespace ComCcsdsSband {
namespace Allocation {
extern Fw::MemAllocator& memAllocator;
}
}  // namespace ComCcsdsSband

namespace ComCcsdsLora {
namespace Allocation {
extern Fw::MemAllocator& memAllocator;
}
}  // namespace ComCcsdsLora

#endif
