#ifndef ONLINE_SDF_RENDERER_H
#define ONLINE_SDF_RENDERER_H
#include "vkutils.h"
#include <optional>
#include <filesystem>
#include <vulkan/vulkan.h>

inline constexpr uint32_t WINDOW_WIDTH = 800;
inline constexpr uint32_t WINDOW_HEIGHT = 600;
inline constexpr char WINDOW_TITLE[] = "Vulkan";
inline constexpr char FULL_SCREEN_QUAD_VERT_SHADER_PATH[] =
    "shaders/fullscreenquad.vert";

struct GLFWApplication {
    bool framebufferResized = false;
};

// Online renderer: Vulkan + swapchain -- meant to be displayed.
class OnlineSDFRenderer {
  private:
    // GLFW Setup
    GLFWApplication app;
    GLFWwindow *window;

    // Vulkan Setup
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties deviceProperties;
    VkSurfaceKHR surface;
    uint32_t graphicsQueueIndex;
    VkDevice logicalDevice;
    VkQueue queue;
    VkQueryPool queryPool = VK_NULL_HANDLE;
    VkSurfaceFormatKHR swapchainFormat;
    VkCommandPool commandPool;
    vkutils::Semaphores imageAvailableSemaphores;
    vkutils::Semaphores renderFinishedSemaphores;
    vkutils::Fences fences;

    // Shader Modules.
    // Full screen quad vert shader + frag shader
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    std::string fragShaderPath;
    bool useToyTemplate;

    // Render Context
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkExtent2D swapchainSize;
    vkutils::SwapchainImages swapchainImages;
    vkutils::SwapchainImageViews swapchainImageViews;
    VkRenderPass renderPass;
    vkutils::FrameBuffers frameBuffers;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    vkutils::CommandBuffers commandBuffers;

    // Runtime configuration
    std::optional<uint32_t> maxFrames;
    bool headless = false;

    // Debug-only: copies the swapchain image before present, which stalls.
    // Mainly useful for smoke tests or debugging.
    std::optional<std::filesystem::path> debugDumpPPMDir;
    uint32_t dumpedFrames = 0;

    // Timing
    std::chrono::time_point<std::chrono::high_resolution_clock> cpuStartFrame,
        cpuEndFrame;

    void glfwSetup();
    void vulkanSetup();
    void setupRenderContext();
    void createCommandBuffers();
    void createPipeline();
    void calcTimestamps(uint32_t imageIndex);
    void destroyRenderContext();
    void destroyPipeline();
    void destroy();

    [[nodiscard]] vkutils::PushConstants
    getPushConstants(uint32_t currentFrame) noexcept;

  public:
    OnlineSDFRenderer(const OnlineSDFRenderer &) = delete;
    OnlineSDFRenderer &operator=(const OnlineSDFRenderer &) = delete;
    OnlineSDFRenderer(const std::string &fragShaderPath,
                      bool useToyTemplate = false,
                      std::optional<uint32_t> maxFrames = std::nullopt,
                      bool headless = false,
                      std::optional<std::filesystem::path> debugDumpPPMDir =
                          std::nullopt);
    void setup();
    void gameLoop();
};

#endif // ONLINE_SDF_RENDERER_H
