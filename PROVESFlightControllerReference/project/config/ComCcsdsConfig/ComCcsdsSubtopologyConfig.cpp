#include "ComCcsdsSubtopologyConfig.hpp"

#include <default/zephyr-config/ZephyrAllocator.hpp>

namespace ComCcsds {
namespace Allocation {
Fw::ZephyrKmallocAllocator allocatorInstance;
Fw::MemAllocator& memAllocator = allocatorInstance;
}  // namespace Allocation
}  // namespace ComCcsds
