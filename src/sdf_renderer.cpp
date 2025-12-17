#include "sdf_renderer.h"
#include "ffmpeg_encoder.h"
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
                         std::optional<uint32_t> maxFrames, bool headless,
                         std::optional<std::string> videoOutputPath)
    : fragShaderPath(fragShaderPath), useToyTemplate(useToyTemplate),
      maxFrames(maxFrames), headless(headless), 
      videoOutputPath(videoOutputPath) {}

void SDFRenderer::setup() {
    glfwSetup();
    vulkanSetup();
    setupRenderContext();
    createPipeline();
    createCommandBuffers();
    if (videoOutputPath) {
        setupVideoEncoder();
        setupReadbackResources();
    }
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
        vkutils::createShaderModule(logicalDevice, vertSpirvPath.string());
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
    } catch (const std::runtime_error&) {
        // An error occured while compiling the shader
        // This can happen while doing live edits
        // Just try find the old one until the error is fixed
        fragSpirvPath = fragShaderPath;
        fragSpirvPath.replace_extension(".spv");
    }
    fragShaderModule =
        vkutils::createShaderModule(logicalDevice, fragSpirvPath.string());
    pipeline = vkutils::createGraphicsPipeline(
        logicalDevice, renderPass, pipelineLayout, swapchainSize,
        vertShaderModule, fragShaderModule);
}

void SDFRenderer::createCommandBuffers() {
    commandBuffers = vkutils::createCommandBuffers(logicalDevice, commandPool,
                                                   swapchainImages.count);
}

void SDFRenderer::setupVideoEncoder() {
    if (!videoOutputPath) {
        return;
    }
    
    spdlog::info("Setting up video encoder for output: {}", *videoOutputPath);
    videoEncoder = std::make_unique<FFmpegEncoder>(
        *videoOutputPath, swapchainSize.width, swapchainSize.height, 30);
    videoEncoder->initialize();
}

void SDFRenderer::setupReadbackResources() {
    if (!videoOutputPath) {
        return;
    }

    // Allocate readback buffer for copying image data from GPU to CPU
    VkDeviceSize bufferSize = static_cast<VkDeviceSize>(swapchainSize.width) * 
                              swapchainSize.height * 4; // RGBA

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &readbackBuffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, readbackBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    // Find memory type that is host visible and host coherent
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    uint32_t memoryTypeIndex = UINT32_MAX;
    VkMemoryPropertyFlags requiredProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties) {
            memoryTypeIndex = i;
            break;
        }
    }

    if (memoryTypeIndex == UINT32_MAX) {
        throw std::runtime_error("Failed to find suitable memory type for readback buffer");
    }

    allocInfo.memoryTypeIndex = memoryTypeIndex;
    VK_CHECK(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &readbackBufferMemory));
    VK_CHECK(vkBindBufferMemory(logicalDevice, readbackBuffer, readbackBufferMemory, 0));

    pixelData.resize(bufferSize);
    spdlog::info("Readback resources setup complete");
}

void SDFRenderer::destroyReadbackResources() {
    if (readbackBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(logicalDevice, readbackBuffer, nullptr);
        readbackBuffer = VK_NULL_HANDLE;
    }
    if (readbackBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(logicalDevice, readbackBufferMemory, nullptr);
        readbackBufferMemory = VK_NULL_HANDLE;
    }
}

void SDFRenderer::captureFrameToVideo(uint32_t imageIndex) {
    if (!videoEncoder || !videoEncoder->isInitialized()) {
        return;
    }

    // Create a command buffer for the copy operation
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer copyCommandBuffer;
    vkAllocateCommandBuffers(logicalDevice, &allocInfo, &copyCommandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(copyCommandBuffer, &beginInfo);

    // Transition swapchain image to transfer source layout
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = swapchainImages.images[imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(copyCommandBuffer,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy image to buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {swapchainSize.width, swapchainSize.height, 1};

    vkCmdCopyImageToBuffer(copyCommandBuffer,
                          swapchainImages.images[imageIndex],
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          readbackBuffer,
                          1, &region);

    // Transition back to present layout
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(copyCommandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(copyCommandBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &copyCommandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // Map memory and copy to pixel data
    void* data;
    vkMapMemory(logicalDevice, readbackBufferMemory, 0, pixelData.size(), 0, &data);
    std::memcpy(pixelData.data(), data, pixelData.size());
    vkUnmapMemory(logicalDevice, readbackBufferMemory);

    // Encode frame
    videoEncoder->encodeFrame(pixelData.data());

    // Free the command buffer
    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &copyCommandBuffer);
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
    vkutils::PushConstants pushConstants;
    pushConstants.iTime = static_cast<float>(glfwGetTime());
    pushConstants.iFrame = currentFrame;
    pushConstants.iResolution = glm::vec2(swapchainSize.width, swapchainSize.height);
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
        
        // Capture frame to video if recording
        if (videoOutputPath) {
            captureFrameToVideo(imageIndex);
        }
        
        frameIndex = (frameIndex + 1) % swapchainImages.count;
        currentFrame++;
        cpuEndFrame = std::chrono::high_resolution_clock::now();
        calcTimestamps(imageIndex);
    }

    filewatcher->stopWatching();
    
    // Finalize video encoder if recording
    if (videoEncoder) {
        videoEncoder->finalize();
    }
    
    spdlog::info("Done!");
    destroy();
}

void SDFRenderer::destroy() {
    VK_CHECK(vkDeviceWaitIdle(logicalDevice));
    destroyReadbackResources();
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
