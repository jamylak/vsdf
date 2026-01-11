#ifndef ONLINE_SDF_RENDERER_H
#define ONLINE_SDF_RENDERER_H
#include "sdf_renderer.h"
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
class OnlineSDFRenderer : public SDFRenderer {
  private:
    // GLFW Setup
    GLFWApplication app;
    GLFWwindow *window;

    // Vulkan Setup
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR swapchainFormat;
    vkutils::Semaphores imageAvailableSemaphores;
    vkutils::Semaphores renderFinishedSemaphores;

    // Shader Modules.
    // Full screen quad vert shader + frag shader
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkExtent2D swapchainSize;
    vkutils::SwapchainImages swapchainImages;
    vkutils::SwapchainImageViews swapchainImageViews;
    vkutils::FrameBuffers frameBuffers;
    bool headless = false;
    std::optional<uint32_t> maxFrames;

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
