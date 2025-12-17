#include "ComCcsdsSubtopologyConfig.hpp"

namespace ComCcsdsLora {
namespace Allocation {
// This instance can be changed to use a different allocator in the ComCcsdsLora Subtopology
Fw::MallocAllocator mallocatorInstance;
Fw::MemAllocator& memAllocator = mallocatorInstance;
}  // namespace Allocation
}  // namespace ComCcsdsLora
