#include "ComCcsdsSubtopologyConfig.hpp"

namespace ComCcsdsSband {
namespace Allocation {
// This instance can be changed to use a different allocator in the ComCcsdsSband Subtopology
Fw::MallocAllocator mallocatorInstance;
Fw::MemAllocator& memAllocator = mallocatorInstance;
}  // namespace Allocation
}  // namespace ComCcsdsSband

namespace ComCcsdsLora {
namespace Allocation {
// This instance can be changed to use a different allocator in the ComCcsdsLora Subtopology
Fw::MallocAllocator mallocatorInstance;
Fw::MemAllocator& memAllocator = mallocatorInstance;
}  // namespace Allocation
}  // namespace ComCcsdsLora
