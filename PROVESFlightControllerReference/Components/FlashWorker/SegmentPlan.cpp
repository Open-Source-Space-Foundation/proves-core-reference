// ======================================================================
// \title  SegmentPlan.cpp
// \brief  cpp file for uplink segment naming and validation helpers
// ======================================================================

#include "SegmentPlan.hpp"

namespace Components {

// Out of line definitions so these can be odr-used by callers, for the same reason as in
// PatchApplier.cpp: not every translation unit here is compiled as C++17.
constexpr uint16_t SegmentPlan::MAX_SEGMENTS;
constexpr uint8_t SegmentPlan::SUFFIX_DIGITS;

bool SegmentPlan ::isValidSegmentCount(uint16_t segments) {
    return (segments > 0) && (segments <= SegmentPlan::MAX_SEGMENTS);
}

size_t SegmentPlan ::requiredNameSize(size_t prefix_length) {
    // prefix + '.' + digits + null terminator
    return prefix_length + 1U + SegmentPlan::SUFFIX_DIGITS + 1U;
}

bool SegmentPlan ::formatSegmentName(const char* prefix, uint16_t index, char* buffer, size_t buffer_size) {
    if ((prefix == nullptr) || (buffer == nullptr) || (buffer_size == 0)) {
        return false;
    }
    // Guarantee a terminated string even on the failure paths below
    buffer[0] = '\0';
    if (index >= SegmentPlan::MAX_SEGMENTS) {
        return false;
    }

    size_t prefix_length = 0;
    while (prefix[prefix_length] != '\0') {
        prefix_length++;
    }
    if (SegmentPlan::requiredNameSize(prefix_length) > buffer_size) {
        return false;
    }

    for (size_t i = 0; i < prefix_length; i++) {
        buffer[i] = prefix[i];
    }
    size_t position = prefix_length;
    buffer[position] = '.';
    position++;

    // Fixed width, zero padded, so that segment names sort in transfer order
    uint16_t remaining = index;
    for (uint8_t digit = 0; digit < SegmentPlan::SUFFIX_DIGITS; digit++) {
        const size_t offset = position + (SegmentPlan::SUFFIX_DIGITS - 1U - digit);
        buffer[offset] = static_cast<char>('0' + (remaining % 10U));
        remaining = static_cast<uint16_t>(remaining / 10U);
    }
    position += SegmentPlan::SUFFIX_DIGITS;
    buffer[position] = '\0';
    return true;
}

}  // namespace Components
