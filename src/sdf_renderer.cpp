#include "sdf_renderer.h"
#include "filewatcher/filewatcher_factory.h"
#include "glfwutils.h"
#include "shader_utils.h"
#include "vkutils.h"
#include <cstdint>
#include <spdlog/spdlog.h>

void framebufferResizeCallback(GLFWwindow *window, int width,
                               int height) noexcept {
    auto app =
        reinterpret_cast<GLFWApplication *>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
    spdlog::info("Framebuffer resized to {}x{}", width, height);
};

SDFRenderer::SDFRenderer(const std::string &fragShaderPath,
                         bool useToyTemplate,
                         std::optional<uint32_t> maxFrames, bool headless)
    : fragShaderPath(fragShaderPath), useToyTemplate(useToyTemplate),
      maxFrames(maxFrames), headless(headless) {}

void SDFRenderer::setup() {
    glfwSetup();
    vulkanSetup();
    setupRenderContext();
    createPipeline();
    createCommandBuffers();
}

void SDFRenderer::glfwSetup() {
    // GLFW Setup
    glfwutils::initGLFW();
    glfwWindowHint(GLFW_VISIBLE, headless ? GLFW_FALSE : GLFW_TRUE);
    window =
        glfwutils::createGLFWwindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    glfwSetWindowUserPointer(window, &app);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void SDFRenderer::vulkanSetup() {
    instance = vkutils::setupVulkanInstance();
    physicalDevice = vkutils::findGPU(instance);
    deviceProperties = vkutils::getDeviceProperties(physicalDevice);
    spdlog::info("Device limits {:.3f}",
                 deviceProperties.limits.timestampPeriod);
    surface = vkutils::createVulkanSurface(instance, window);
    graphicsQueueIndex =
        vkutils::getVulkanGraphicsQueueIndex(physicalDevice, surface);
    logicalDevice =
        vkutils::createVulkanLogicalDevice(physicalDevice, graphicsQueueIndex);
    queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(logicalDevice, graphicsQueueIndex, 0, &queue);
    swapchainFormat = vkutils::selectSwapchainFormat(physicalDevice, surface);
    renderPass =
        vkutils::createRenderPass(logicalDevice, swapchainFormat.format);
    commandPool = vkutils::createCommandPool(logicalDevice, graphicsQueueIndex);
    // Since it's SDF, only need to set up full screen quad vert shader once
    std::filesystem::path vertSpirvPath{
        shader_utils::compile(FULL_SCREEN_QUAD_VERT_SHADER_PATH)};
    vertShaderModule =
        vkutils::createShaderModule(logicalDevice, vertSpirvPath);
}

void SDFRenderer::setupRenderContext() {
    spdlog::info("Setting up render context");
    surfaceCapabilities =
        vkutils::getSurfaceCapabilities(physicalDevice, surface);
    swapchainSize = vkutils::getSwapchainSize(window, surfaceCapabilities);
    auto oldSwapchain = swapchain;
    swapchain = vkutils::createSwapchain(physicalDevice, logicalDevice, surface,
                                         surfaceCapabilities, swapchainSize,
                                         swapchainFormat, oldSwapchain);
    if (oldSwapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(logicalDevice, oldSwapchain, nullptr);
    swapchainImages = vkutils::getSwapchainImages(logicalDevice, swapchain);
    if (queryPool == VK_NULL_HANDLE)
        queryPool =
            vkutils::createQueryPool(logicalDevice, swapchainImages.count);
    if (imageAvailableSemaphores.count == 0) {
        imageAvailableSemaphores =
            vkutils::createSemaphores(logicalDevice, swapchainImages.count);
        renderFinishedSemaphores =
            vkutils::createSemaphores(logicalDevice, swapchainImages.count);
        fences = vkutils::createFences(logicalDevice, swapchainImages.count);
    }
    swapchainImageViews = vkutils::createSwapchainImageViews(
        logicalDevice, swapchainFormat, swapchainImages);
    frameBuffers = vkutils::createFrameBuffers(
        logicalDevice, renderPass, swapchainSize, swapchainImageViews);
}

void SDFRenderer::createPipeline() {
    pipelineLayout = vkutils::createPipelineLayout(logicalDevice);
    std::filesystem::path fragSpirvPath;
    try {
        fragSpirvPath = shader_utils::compile(fragShaderPath, useToyTemplate);
    } catch (const std::runtime_error &e) {
        // An error occured while compiling the shader
        // This can happen while doing live edits
        // Just try find the old one until the error is fixed
        fragSpirvPath = fragShaderPath;
        fragSpirvPath.replace_extension(".spv");
    }
    fragShaderModule =
        vkutils::createShaderModule(logicalDevice, fragSpirvPath);
    pipeline = vkutils::createGraphicsPipeline(
        logicalDevice, renderPass, pipelineLayout, swapchainSize,
        vertShaderModule, fragShaderModule);
}

void SDFRenderer::createCommandBuffers() {
    commandBuffers = vkutils::createCommandBuffers(logicalDevice, commandPool,
                                                   swapchainImages.count);
}

void SDFRenderer::destroyPipeline() {
    vkDestroyPipeline(logicalDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
}

void SDFRenderer::destroyRenderContext() {
    VK_CHECK(vkDeviceWaitIdle(logicalDevice));
    VK_CHECK(vkResetCommandPool(logicalDevice, commandPool, 0));
    vkutils::destroyFrameBuffers(logicalDevice, frameBuffers);
    vkutils::destroySwapchainImageViews(logicalDevice, swapchainImageViews);
    // Swapchain gets destroyed after passing oldSwapchain to createSwapchain
}

[[nodiscard]] vkutils::PushConstants
SDFRenderer::getPushConstants(uint32_t currentFrame) noexcept {
    vkutils::PushConstants pushConstants = {
        .iTime = static_cast<float>(glfwGetTime()),
        .iFrame = currentFrame,
        .iResolution = glm::vec2(swapchainSize.width, swapchainSize.height),
    };
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        pushConstants.iMouse = glm::vec2{xpos, ypos};
    } else {
        pushConstants.iMouse = glm::vec2{-1000, -1000};
    }
    return pushConstants;
}

void SDFRenderer::calcTimestamps(uint32_t imageIndex) {
    // Get GPU Time
    uint64_t timestamps[2];
    vkGetQueryPoolResults(logicalDevice, queryPool, imageIndex * 2, 2,
                          sizeof(timestamps), &timestamps, sizeof(uint64_t),
                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    // deviceProperties.limits.timestampPeriod is
    // the number of nanoseconds required for a timestamp query
    // to be incremented by 1
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html
    double totalGpuTime =
        static_cast<double>(timestamps[1] - timestamps[0]) *
        static_cast<double>(deviceProperties.limits.timestampPeriod) * 1e-6;

    auto cpuDuration =
        std::chrono::duration<double, std::milli>(cpuEndFrame - cpuStartFrame)
            .count();

    std::string title = fmt::format("VSDF - CPU: {:.3f}ms  GPU: {:.3f}ms",
                                    cpuDuration, totalGpuTime);
    glfwSetWindowTitle(window, title.c_str());
}

void SDFRenderer::gameLoop() {
    uint32_t currentFrame = 0;
    uint32_t frameIndex = 0;
    bool pipelineUpdated = false;
    bool sabotageTriggered = false;
    auto filewatcher = filewatcher_factory::createFileWatcher();
    filewatcher->startWatching(fragShaderPath,
                               [&]() { pipelineUpdated = true; });
    while (!glfwWindowShouldClose(window)) {
        if (maxFrames && currentFrame >= *maxFrames) {
            spdlog::info("Reached max frames {}, exiting.", *maxFrames);
            break;
        }
        cpuStartFrame = std::chrono::high_resolution_clock::now();
        glfwPollEvents();
        uint32_t imageIndex;
        if (app.framebufferResized) {
            destroyRenderContext();
            setupRenderContext();
            app.framebufferResized = false;
            frameIndex = 0;
            spdlog::info("Framebuffer resized!");
        }
        if (pipelineUpdated) {
            spdlog::info("Recreating pipeline");
            VK_CHECK(vkDeviceWaitIdle(logicalDevice));
            destroyPipeline();
            createPipeline();
            pipelineUpdated = false;
        }

        if (!sabotageTriggered && currentFrame >= 50) {
            // Deliberately invalidate synchronization after 50 frames to
            // ensure smoke tests flag broken Vulkan usage.
            spdlog::error(
                "Sabotage: destroying fences and semaphores mid-frame.");
            vkutils::destroySemaphores(logicalDevice,
                                       imageAvailableSemaphores);
            vkutils::destroySemaphores(logicalDevice,
                                       renderFinishedSemaphores);
            vkutils::destroyFences(logicalDevice, fences);
            sabotageTriggered = true;
        }

        VK_CHECK(vkWaitForFences(logicalDevice, 1, &fences.fences[frameIndex],
                                 VK_TRUE, UINT64_MAX));

        VK_CHECK(vkAcquireNextImageKHR(
            logicalDevice, swapchain, UINT64_MAX,
            imageAvailableSemaphores.semaphores[frameIndex], VK_NULL_HANDLE,
            &imageIndex));

        VK_CHECK(vkResetFences(logicalDevice, 1, &fences.fences[frameIndex]));
        vkutils::recordCommandBuffer(
            queryPool, renderPass, swapchainSize, pipeline, pipelineLayout,
            commandBuffers.commandBuffers[imageIndex],
            frameBuffers.framebuffers[imageIndex],
            getPushConstants(currentFrame), imageIndex);
        vkutils::submitCommandBuffer(
            queue, commandBuffers.commandBuffers[imageIndex],
            imageAvailableSemaphores.semaphores[imageIndex],
            renderFinishedSemaphores.semaphores[imageIndex],
            fences.fences[frameIndex]);
        vkutils::presentImage(queue, swapchain,
                              renderFinishedSemaphores.semaphores[frameIndex],
                              imageIndex);
        frameIndex = (frameIndex + 1) % swapchainImages.count;
        currentFrame++;
        cpuEndFrame = std::chrono::high_resolution_clock::now();
        calcTimestamps(imageIndex);
    }

    filewatcher->stopWatching();
    spdlog::info("Done!");
    destroy();
}

void SDFRenderer::destroy() {
    VK_CHECK(vkDeviceWaitIdle(logicalDevice));
    vkutils::destroySemaphores(logicalDevice, imageAvailableSemaphores);
    vkutils::destroySemaphores(logicalDevice, renderFinishedSemaphores);
    vkutils::destroyFences(logicalDevice, fences);
    vkDestroyPipeline(logicalDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
    vkutils::destroyFrameBuffers(logicalDevice, frameBuffers);
    vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
    vkutils::destroySwapchainImageViews(logicalDevice, swapchainImageViews);
    vkDestroySwapchainKHR(logicalDevice, swapchain, nullptr);
    vkDestroyQueryPool(logicalDevice, queryPool, nullptr);
    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
}
