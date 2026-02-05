#include "ComCcsdsSubtopologyConfig.hpp"

#include <Fw/Types/Assert.hpp>

#include <zephyr/kernel.h>

namespace {

bool isPowerOfTwo(const FwSizeType value) {
    return (value != 0U) && ((value & (value - 1U)) == 0U);
}

FwSizeType roundUpToPowerOfTwo(FwSizeType value) {
    if (value <= 1U) {
        return 1U;
    }

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    if (sizeof(FwSizeType) > 4U) {
        value |= value >> 32;
    }
    value++;
    return value;
}

class ZephyrKmallocAllocator final : public Fw::MemAllocator {
  public:
    void* allocate(const FwEnumStoreType identifier,
                   FwSizeType& size,
                   bool& recoverable,
                   FwSizeType alignment = alignof(std::max_align_t)) override {
        static_cast<void>(identifier);
        recoverable = false;

        const FwSizeType minAlignment = static_cast<FwSizeType>(sizeof(void*));
        FwSizeType requestedAlignment = (alignment < minAlignment) ? minAlignment : alignment;
        if (!isPowerOfTwo(requestedAlignment)) {
            requestedAlignment = roundUpToPowerOfTwo(requestedAlignment);
        }
        FW_ASSERT(isPowerOfTwo(requestedAlignment), static_cast<FwAssertArgType>(requestedAlignment));

        void* mem = (requestedAlignment <= minAlignment) ? k_malloc(size) : k_aligned_alloc(requestedAlignment, size);
        if (mem == nullptr) {
            size = 0;
        }
        return mem;
    }

    void deallocate(const FwEnumStoreType identifier, void* ptr) override {
        static_cast<void>(identifier);
        k_free(ptr);
    }
};

}  // namespace

namespace ComCcsds {
namespace Allocation {
ZephyrKmallocAllocator allocatorInstance;
Fw::MemAllocator& memAllocator = allocatorInstance;
}  // namespace Allocation
}  // namespace ComCcsds
