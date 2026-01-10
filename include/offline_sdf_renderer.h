#ifndef OFFLINE_SDF_RENDERER_H
#define OFFLINE_SDF_RENDERER_H
#include "sdf_renderer.h"
#include "vkutils.h"
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vulkan/vulkan.h>

inline constexpr uint32_t OFFSCREEN_DEFAULT_WIDTH = 800;
inline constexpr uint32_t OFFSCREEN_DEFAULT_HEIGHT = 600;
inline constexpr char OFFSCREEN_DEFAULT_VERT_SHADER_PATH[] =
    "shaders/fullscreenquad.vert";

// Offline SDF Renderer
// This basis will be used for FFMPEG integration
class OfflineSDFRenderer : public SDFRenderer {
  private:
    // Render Context
    VkExtent2D imageSize{};
    VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkImage offscreenImage = VK_NULL_HANDLE;
    VkDeviceMemory offscreenImageMemory = VK_NULL_HANDLE;
    VkImageView offscreenImageView = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

    void vulkanSetup();
    void setupRenderContext();
    void createPipeline();
    void createCommandBuffers();
    void destroyRenderContext();
    void destroyPipeline();
    void destroy();

    void transitionImageLayout(VkImageLayout oldLayout,
                               VkImageLayout newLayout);
    [[nodiscard]] ReadbackFrame readbackOffscreenImage();
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
        uint32_t height = OFFSCREEN_DEFAULT_HEIGHT);
    void setup();
    void renderFrames();
};

#endif // OFFLINE_SDF_RENDERER_H
