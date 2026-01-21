#include "online_sdf_renderer.h"
#include "filewatcher/filewatcher_factory.h"
#include "glfwutils.h"
#include "shader_utils.h"
#include "vkutils.h"
#include <atomic>
#include <cstdint>
#include <spdlog/spdlog.h>

void framebufferResizeCallback(GLFWwindow *window, int width,
                               int height) noexcept {
    auto app =
        reinterpret_cast<GLFWApplication *>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
    spdlog::info("Framebuffer resized to {}x{}", width, height);
};

OnlineSDFRenderer::OnlineSDFRenderer(
    const std::string &fragShaderPath, bool useToyTemplate,
    std::optional<uint32_t> maxFrames, bool headless,
    std::optional<std::filesystem::path> debugDumpPPMDir, bool noFocus)
    : SDFRenderer(fragShaderPath, useToyTemplate, debugDumpPPMDir),
      headless(headless), noFocus(noFocus), maxFrames(maxFrames) {}

void OnlineSDFRenderer::setup() {
    glfwSetup();
    vulkanSetup();
    setupRenderContext();
    createPipeline();
    createCommandBuffers();
}

void OnlineSDFRenderer::glfwSetup() {
    // GLFW Setup
    glfwutils::initGLFW();
    glfwWindowHint(GLFW_VISIBLE, headless ? GLFW_FALSE : GLFW_TRUE);
    if (noFocus) {
        glfwWindowHint(GLFW_FLOATING, GLFW_TRUE); // Always on top
        glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    }
    window =
        glfwutils::createGLFWwindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    glfwSetWindowUserPointer(window, &app);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void OnlineSDFRenderer::vulkanSetup() {
    instance = vkutils::setupVulkanInstance();
    physicalDevice = vkutils::findGPU(instance);
    deviceProperties = vkutils::getDeviceProperties(physicalDevice);
    logDeviceLimits();
    surface = vkutils::createVulkanSurface(instance, window);
    graphicsQueueIndex =
        vkutils::getVulkanGraphicsQueueIndex(physicalDevice, surface);
    logicalDevice =
        vkutils::createVulkanLogicalDevice(physicalDevice, graphicsQueueIndex);
    queue = VK_NULL_HANDLE;
    initDeviceQueue();
    swapchainFormat = vkutils::selectSwapchainFormat(physicalDevice, surface);
    renderPass =
        vkutils::createRenderPass(logicalDevice, swapchainFormat.format);
    commandPool = vkutils::createCommandPool(logicalDevice, graphicsQueueIndex);
    // Since it's SDF, only need to set up full screen quad vert shader once
    auto vertSpirv = shader_utils::compileFullscreenQuadVertSpirv();
    vertShaderModule = vkutils::createShaderModule(logicalDevice, vertSpirv);
}

void OnlineSDFRenderer::setupRenderContext() {
    spdlog::info("Setting up render context");
    surfaceCapabilities =
        vkutils::getSurfaceCapabilities(physicalDevice, surface);
    swapchainSize = vkutils::getSwapchainSize(window, surfaceCapabilities);
    auto oldSwapchain = swapchain;
    vkutils::SwapchainConfig swapchainConfig{
        .surface = surface,
        .surfaceCapabilities = surfaceCapabilities,
        .extent = swapchainSize,
        .surfaceFormat = swapchainFormat,
        .oldSwapchain = oldSwapchain,
        .enableReadback = debugDumpPPMDir.has_value(),
    };
    swapchain = vkutils::createSwapchain(physicalDevice, logicalDevice,
                                         swapchainConfig);
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

void OnlineSDFRenderer::createPipeline() {
    createPipelineLayoutCommon();
    auto fragSpirv =
        shader_utils::compileFileToSpirv(fragShaderPath, useToyTemplate);
    fragShaderModule = vkutils::createShaderModule(logicalDevice, fragSpirv);
    pipeline = vkutils::createGraphicsPipeline(
        logicalDevice, renderPass, pipelineLayout, swapchainSize,
        vertShaderModule, fragShaderModule);
}

void OnlineSDFRenderer::tryRecreatePipeline() {
    std::vector<uint32_t> fragSpirv;
    try {
        fragSpirv =
            shader_utils::compileFileToSpirv(fragShaderPath, useToyTemplate);
    } catch (const std::runtime_error &err) {
        spdlog::warn("Shader compile failed, keeping previous pipeline: {}",
                     err.what());
        return;
    }

    VK_CHECK(vkDeviceWaitIdle(logicalDevice));
    destroyPipeline();
    createPipelineLayoutCommon();
    fragShaderModule = vkutils::createShaderModule(logicalDevice, fragSpirv);
    pipeline = vkutils::createGraphicsPipeline(
        logicalDevice, renderPass, pipelineLayout, swapchainSize,
        vertShaderModule, fragShaderModule);
}

void OnlineSDFRenderer::createCommandBuffers() {
    commandBuffers = vkutils::createCommandBuffers(logicalDevice, commandPool,
                                                   swapchainImages.count);
}

void OnlineSDFRenderer::destroyPipeline() {
    vkDestroyPipeline(logicalDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
}

void OnlineSDFRenderer::destroyRenderContext() {
    VK_CHECK(vkDeviceWaitIdle(logicalDevice));
    VK_CHECK(vkResetCommandPool(logicalDevice, commandPool, 0));
    vkutils::destroyFrameBuffers(logicalDevice, frameBuffers);
    vkutils::destroySwapchainImageViews(logicalDevice, swapchainImageViews);
    // Swapchain gets destroyed after passing oldSwapchain to createSwapchain
}

[[nodiscard]] vkutils::PushConstants
OnlineSDFRenderer::getPushConstants(uint32_t currentFrame) noexcept {
    vkutils::PushConstants pushConstants = buildPushConstants(
        static_cast<float>(glfwGetTime()), currentFrame,
        glm::vec2(swapchainSize.width, swapchainSize.height));
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        pushConstants.iMouse = glm::vec2{xpos, ypos};
    }
    return pushConstants;
}

void OnlineSDFRenderer::calcTimestamps(uint32_t imageIndex) {
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

void OnlineSDFRenderer::gameLoop() {
    uint32_t currentFrame = 0;
    uint32_t frameIndex = 0;
    std::atomic<bool> pipelineUpdated{false};
    auto filewatcher = filewatcher_factory::createFileWatcher();
    filewatcher->startWatching(fragShaderPath, [&]() {
        pipelineUpdated.store(true, std::memory_order_relaxed);
    });
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
        if (pipelineUpdated.exchange(false, std::memory_order_relaxed)) {
            spdlog::info("Recreating pipeline");
            tryRecreatePipeline();
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
        if (debugDumpPPMDir) {
            // Debug-only: copy the swapchain image before present, which
            // stalls. Mainly useful for smoke tests or debugging.
            VK_CHECK(vkWaitForFences(logicalDevice, 1,
                                     &fences.fences[frameIndex], VK_TRUE,
                                     UINT64_MAX));
            vkutils::ReadbackContext readbackContext{};
            readbackContext.device = logicalDevice;
            readbackContext.physicalDevice = physicalDevice;
            readbackContext.commandPool = commandPool;
            readbackContext.queue = queue;
            PPMDebugFrame frame = vkutils::debugReadbackSwapchainImage(
                readbackContext, swapchainImages.images[imageIndex],
                swapchainFormat.format, swapchainSize);
            dumpDebugFrame(frame);
        }
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

void OnlineSDFRenderer::destroy() {
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
