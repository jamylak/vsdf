#ifndef SDF_RENDERER_H
#define SDF_RENDERER_H

#include "vkutils.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vulkan/vulkan.h>

class SDFRenderer {
  protected:
    SDFRenderer(const std::string &fragShaderPath, bool useToyTemplate,
                std::optional<std::filesystem::path> debugDumpPPMDir);

    void logDeviceLimits() const;
    void initDeviceQueue();
    void createPipelineLayoutCommon();
    void dumpDebugFrame(const ReadbackFrame &frame);
    void destroyPipelineCommon() noexcept;
    [[nodiscard]] vkutils::PushConstants
    buildPushConstants(float timeSeconds, uint32_t currentFrame,
                       const glm::vec2 &resolution) const noexcept;

    // Vulkan Setup
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties deviceProperties{};
    uint32_t graphicsQueueIndex = 0;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkQueryPool queryPool = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    // Shader Modules.
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;
    std::string fragShaderPath;

    // Whether to use shader toy template
    // eg. old school OpenGL style shaders
    // + some things like iTime etc..
    bool useToyTemplate = false;

    // Render Context
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    vkutils::CommandBuffers commandBuffers;
    vkutils::Fences fences;

    // Some useful stuff to debug
    std::optional<std::filesystem::path> debugDumpPPMDir;
    uint32_t dumpedFrames = 0;
};

#endif // SDF_RENDERER_H
