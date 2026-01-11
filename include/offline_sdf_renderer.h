#ifndef OFFLINE_SDF_RENDERER_H
#define OFFLINE_SDF_RENDERER_H
#include "sdf_renderer.h"
#include "ffmpeg_encode_settings.h"
#include "ffmpeg_encoder.h"
#include "vkutils.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
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
        void *mappedData = nullptr;
        uint32_t rowStride = 0;
        bool pendingReadback = false;
        bool pendingEncode = false;
    };

    // Render Context
    VkExtent2D imageSize{};
    VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    vkutils::ReadbackFormatInfo readbackFormatInfo{};

    // Ring buffer timing intuition:
    //  - 1 slot: total ≈ N * (render + readback) (no overlap).
    //  - K >= 2: total ≈ (render + readback) + (N - 1) * max(render, readback).
    const uint32_t ringSize = OFFSCREEN_DEFAULT_RING_SIZE;
    std::array<RingSlot, MAX_FRAME_SLOTS> ringSlots;

    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

    static uint32_t validateRingSize(uint32_t value);

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

    struct EncodeItem {
        uint32_t slotIndex = 0;
        uint32_t frameIndex = 0;
    };

    ffmpeg_utils::EncodeSettings encodeSettings;
    std::unique_ptr<ffmpeg_utils::FfmpegEncoder> encoder;
    std::thread encoderThread;
    std::mutex encodeMutex;
    std::condition_variable encodeCv;
    std::deque<EncodeItem> encodeQueue;
    bool encodeStop = false;
    bool encodeFailed = false;

    void startEncoding();
    void stopEncoding();
    void enqueueEncode(uint32_t slotIndex, uint32_t frameIndex);
    void waitForSlotEncode(uint32_t slotIndex);

  public:
    OfflineSDFRenderer(const OfflineSDFRenderer &) = delete;
    OfflineSDFRenderer &operator=(const OfflineSDFRenderer &) = delete;
    OfflineSDFRenderer(
        const std::string &fragShaderPath, uint32_t maxFrames,
        bool useToyTemplate = false,
        std::optional<std::filesystem::path> debugDumpPPMDir = std::nullopt,
        uint32_t width = OFFSCREEN_DEFAULT_WIDTH,
        uint32_t height = OFFSCREEN_DEFAULT_HEIGHT,
        uint32_t ringSize = OFFSCREEN_DEFAULT_RING_SIZE,
        ffmpeg_utils::EncodeSettings encodeSettings);
    void setup();
    void renderFrames();
};

#endif // OFFLINE_SDF_RENDERER_H
