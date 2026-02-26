#include "ComCcsdsSubtopologyConfig.hpp"

namespace ComCcsds {
namespace Allocation {
Fw::ZephyrKmallocAllocator allocatorInstance;
Fw::MemAllocator& memAllocator = allocatorInstance;
}  // namespace Allocation
}  // namespace ComCcsds
