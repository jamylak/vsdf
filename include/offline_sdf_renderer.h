#ifndef OFFLINE_SDF_RENDERER_H
#define OFFLINE_SDF_RENDERER_H
#include "sdf_renderer.h"
#include "vkutils.h"
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vulkan/vulkan.h>

inline constexpr uint32_t OFFSCREEN_DEFAULT_WIDTH = 1280;
inline constexpr uint32_t OFFSCREEN_DEFAULT_HEIGHT = 720;
inline constexpr uint32_t OFFSCREEN_DEFAULT_RING_SIZE = 2;
inline constexpr char OFFSCREEN_DEFAULT_VERT_SHADER_PATH[] =
    "shaders/fullscreenquad.vert";

// Offline SDF Renderer
// This basis will be used for FFMPEG integration
class OfflineSDFRenderer : public SDFRenderer {
  private:
    struct RingSlot {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        vkutils::ReadbackBuffer stagingBuffer{};
        bool pendingReadback = false;
    };

    // Render Context
    VkExtent2D imageSize{};
    VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    const uint32_t ringSize = OFFSCREEN_DEFAULT_RING_SIZE;
    std::array<RingSlot, MAX_FRAME_SLOTS> ringSlots;

    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

    void vulkanSetup();
    void setupRenderContext();
    void createPipeline();
    void createCommandBuffers();
    void destroyRenderContext();
    void destroyPipeline();
    void destroy();

    void transitionImageLayout(VkImage image, VkImageLayout oldLayout,
                               VkImageLayout newLayout);
    void recordCommandBuffer(uint32_t slotIndex, uint32_t currentFrame);
    [[nodiscard]] ReadbackFrame readbackOffscreenImage(const RingSlot &slot);
    [[nodiscard]] vkutils::PushConstants
    getPushConstants(uint32_t currentFrame) noexcept;

  public:
    OfflineSDFRenderer(const OfflineSDFRenderer &) = delete;
    OfflineSDFRenderer &operator=(const OfflineSDFRenderer &) = delete;
    OfflineSDFRenderer(
        const std::string &fragShaderPath, bool useToyTemplate = false,
        std::optional<uint32_t> maxFrames = std::nullopt,
        std::optional<std::filesystem::path> debugDumpPPMDir = std::nullopt,
        uint32_t width = OFFSCREEN_DEFAULT_WIDTH,
        uint32_t height = OFFSCREEN_DEFAULT_HEIGHT,
        uint32_t ringSize = OFFSCREEN_DEFAULT_RING_SIZE);
    void setup();
    void renderFrames();
};

#endif // OFFLINE_SDF_RENDERER_H
