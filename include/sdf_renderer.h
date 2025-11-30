#ifndef SDF_RENDERER_H
#define SDF_RENDERER_H
#include "vkutils.h"
#include <vulkan/vulkan.h>

inline constexpr uint WINDOW_WIDTH = 800;
inline constexpr uint WINDOW_HEIGHT = 600;
inline constexpr char WINDOW_TITLE[] = "Vulkan";
inline constexpr char FULL_SCREEN_QUAD_VERT_SHADER_PATH[] =
    "shaders/fullscreenquad.vert";

struct GLFWApplication {
    bool framebufferResized = false;
};

class SDFRenderer {
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
    VkPushConstantRange pushConstantRange;
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
    SDFRenderer(const SDFRenderer &) = delete;
    SDFRenderer &operator=(const SDFRenderer &) = delete;
    SDFRenderer(const std::string &fragShaderPath, bool useToyTemplate = false);
    void setup();
    void gameLoop();
};

#endif // SDF_RENDERER_H
