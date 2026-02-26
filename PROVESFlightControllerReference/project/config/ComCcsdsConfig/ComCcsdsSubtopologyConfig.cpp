#include "ComCcsdsSubtopologyConfig.hpp"

#include <config/MemoryAllocation.hpp>

namespace ComCcsds {
namespace Allocation {
Fw::ZephyrKmallocAllocator allocatorInstance;
Fw::MemAllocator& memAllocator = allocatorInstance;
}  // namespace Allocation
}  // namespace ComCcsds
