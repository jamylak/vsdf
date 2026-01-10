#ifndef OFFLINE_SDF_UTILS_H
#define OFFLINE_SDF_UTILS_H

#include <cstdint>
#include <stdexcept>
#include <vulkan/vulkan.h>

namespace offline_sdf_utils {
struct ReadbackFormatInfo {
    uint32_t bytesPerPixel = 0;
    bool swapRB = false;
};

[[nodiscard]] inline ReadbackFormatInfo getReadbackFormatInfo(VkFormat format) {
    switch (format) {
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        return {4, true};
    default:
        throw std::runtime_error(
            "Unsupported offscreen format for readback; expected BGRA8");
    }
}
} // namespace offline_sdf_utils

#endif // OFFLINE_SDF_UTILS_H
