#include "offline_sdf_renderer.h"
#include "shader_utils.h"
#include "vkutils.h"
#include <cstdint>
#include <spdlog/spdlog.h>

OfflineSDFRenderer::OfflineSDFRenderer(
    const std::string &fragShaderPath, bool useToyTemplate,
    std::optional<uint32_t> maxFrames,
    std::optional<std::filesystem::path> debugDumpPPMDir, uint32_t width,
    uint32_t height, uint32_t ringSize)
    : SDFRenderer(fragShaderPath, useToyTemplate, maxFrames, debugDumpPPMDir),
      imageSize({width, height}),
      ringSize(ringSize) {}

void OfflineSDFRenderer::setup() {
    vulkanSetup();
    setupRenderContext();
    createPipeline();
    createCommandBuffers();
    startTime = std::chrono::high_resolution_clock::now();
}

void OfflineSDFRenderer::vulkanSetup() {
    instance = vkutils::setupVulkanInstance(true);
    physicalDevice = vkutils::findGPU(instance);
    deviceProperties = vkutils::getDeviceProperties(physicalDevice);
    logDeviceLimits();
    graphicsQueueIndex = vkutils::getVulkanGraphicsQueueIndex(physicalDevice);
    logicalDevice = vkutils::createVulkanLogicalDevice(
        physicalDevice, graphicsQueueIndex, true);
    initDeviceQueue();
    renderPass = vkutils::createRenderPass(logicalDevice, imageFormat, true);
    commandPool = vkutils::createCommandPool(logicalDevice, graphicsQueueIndex);

    std::filesystem::path vertSpirvPath{
        shader_utils::compile(OFFSCREEN_DEFAULT_VERT_SHADER_PATH)};
    vertShaderModule =
        vkutils::createShaderModule(logicalDevice, vertSpirvPath.string());
}

void OfflineSDFRenderer::setupRenderContext() {
    if (ringSize == 0)
        ringSize = 1;
    if (ringSize > MAX_FRAME_SLOTS) {
        spdlog::warn("Ring size {} exceeds max {}; clamping.", ringSize,
                     MAX_FRAME_SLOTS);
        ringSize = MAX_FRAME_SLOTS;
    }

    const auto formatInfo = vkutils::getReadbackFormatInfo(imageFormat);
    VkDeviceSize imageBytes = static_cast<VkDeviceSize>(imageSize.width) *
                              static_cast<VkDeviceSize>(imageSize.height) *
                              formatInfo.bytesPerPixel;

    VkImageCreateInfo imageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = imageFormat,
        .extent = {imageSize.width, imageSize.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImageViewCreateInfo imageViewCreateInfoTemplate{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = imageFormat,
        // Image will be filled in later to be offscreen image
        // .image = ... ring slot image ...
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkFramebufferCreateInfo framebufferInfoTemplate{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass,
        .attachmentCount = 1,
        // Attachment will be filled later to be offscreen image view
        // .pAttachments = ... ring slot image view ...
        .width = imageSize.width,
        .height = imageSize.height,
        .layers = 1,
    };

    VK_CHECK(vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr,
                                 &framebuffer));

    if (queryPool == VK_NULL_HANDLE) {
        queryPool = vkutils::createQueryPool(logicalDevice, 1);
    }
    if (fences.count == 0) {
        fences = vkutils::createFences(logicalDevice, 1);
    }

    transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

void OfflineSDFRenderer::createPipeline() {
    createPipelineLayoutCommon();
    std::filesystem::path fragSpirvPath =
        shader_utils::compile(fragShaderPath, useToyTemplate);
    fragShaderModule =
        vkutils::createShaderModule(logicalDevice, fragSpirvPath.string());
    pipeline = vkutils::createGraphicsPipeline(
        logicalDevice, renderPass, pipelineLayout, imageSize, vertShaderModule,
        fragShaderModule);
}

void OfflineSDFRenderer::createCommandBuffers() {
    commandBuffers =
        vkutils::createCommandBuffers(logicalDevice, commandPool, 1);
}

void OfflineSDFRenderer::transitionImageLayout(VkImageLayout oldLayout,
                                               VkImageLayout newLayout) {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer commandBuffer;
    // One-time command buffer to record a single layout transition.
    VK_CHECK(
        vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = offscreenImage,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        // No need to wait on prior writes; we don't care about old contents.
        barrier.srcAccessMask = 0;
        // Make color-attachment writes visible for subsequent render pass use.
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    } else {
        throw std::runtime_error("Unsupported image layout transition");
    }

    // Insert the layout transition with matching pipeline stage scopes.
    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    // Submit and wait so the image is ready before further use.
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));
    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

vkutils::PushConstants
OfflineSDFRenderer::getPushConstants(uint32_t currentFrame) noexcept {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<float>(now - startTime).count();
    return buildPushConstants(elapsed, currentFrame,
                              glm::vec2(imageSize.width, imageSize.height));
}

ReadbackFrame OfflineSDFRenderer::readbackOffscreenImage() {
    const auto formatInfo = vkutils::getReadbackFormatInfo(imageFormat);

    VkDeviceSize imageBytes = static_cast<VkDeviceSize>(imageSize.width) *
                              static_cast<VkDeviceSize>(imageSize.height) *
                              formatInfo.bytesPerPixel;
    // Staging buffer for GPU->CPU readback.
    vkutils::ReadbackBuffer stagingBuffer =
        vkutils::createReadbackBuffer(logicalDevice, physicalDevice, imageBytes,
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer commandBuffer;
    // One-time command buffer to handle layout transitions + copy.
    VK_CHECK(
        vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    // Transition offscreen image to transfer-src layout.
    // so we can copy from it.
    VkImageMemoryBarrier barrierToTransfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = offscreenImage,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrierToTransfer);

    VkBufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset = {0, 0, 0},
        .imageExtent = {imageSize.width, imageSize.height, 1},
    };

    vkCmdCopyImageToBuffer(commandBuffer, offscreenImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           stagingBuffer.buffer, 1, &region);

    // Transition back to color-attachment layout for future rendering.
    VkImageMemoryBarrier barrierToColor{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = offscreenImage,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &barrierToColor);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    // Wait so the CPU sees a fully populated staging buffer.
    VK_CHECK(vkQueueWaitIdle(queue));

    void *data = nullptr;
    VK_CHECK(vkMapMemory(logicalDevice, stagingBuffer.memory, 0, imageBytes, 0,
                         &data));

    ReadbackFrame frame;
    frame.allocateRGB(imageSize.width, imageSize.height);
    const uint8_t *src = static_cast<const uint8_t *>(data);
    const size_t pixelCount = static_cast<size_t>(imageSize.width) *
                              static_cast<size_t>(imageSize.height);
    for (size_t i = 0; i < pixelCount; ++i) {
        const size_t srcOffset = i * formatInfo.bytesPerPixel;
        const size_t dstOffset = i * 3;
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        // Normalize to RGB (no alpha) with optional channel swizzle.
        if (formatInfo.swapRB) {
            r = src[srcOffset + 2];
            g = src[srcOffset + 1];
            b = src[srcOffset + 0];
        } else {
            r = src[srcOffset + 0];
            g = src[srcOffset + 1];
            b = src[srcOffset + 2];
        }
        frame.rgb[dstOffset + 0] = r;
        frame.rgb[dstOffset + 1] = g;
        frame.rgb[dstOffset + 2] = b;
    }

    vkUnmapMemory(logicalDevice, stagingBuffer.memory);
    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
    vkutils::destroyReadbackBuffer(logicalDevice, stagingBuffer);

    return frame;
}

void OfflineSDFRenderer::renderFrames() {
    uint32_t totalFrames = maxFrames.value_or(1);
    for (uint32_t currentFrame = 0; currentFrame < totalFrames;
         ++currentFrame) {
        VK_CHECK(vkWaitForFences(logicalDevice, 1, &fences.fences[0], VK_TRUE,
                                 UINT64_MAX));
        VK_CHECK(vkResetFences(logicalDevice, 1, &fences.fences[0]));

        vkutils::recordCommandBuffer(
            queryPool, renderPass, imageSize, pipeline, pipelineLayout,
            commandBuffers.commandBuffers[0], framebuffer,
            getPushConstants(currentFrame), 0);

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers.commandBuffers[0],
        };
        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fences.fences[0]));
        VK_CHECK(vkWaitForFences(logicalDevice, 1, &fences.fences[0], VK_TRUE,
                                 UINT64_MAX));

        if (debugDumpPPMDir) {
            ReadbackFrame frame = readbackOffscreenImage();
            dumpDebugFrame(frame);
        }
    }

    spdlog::info("Offline render done.");
    destroy();
}

void OfflineSDFRenderer::destroyPipeline() {
    vkDestroyPipeline(logicalDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
}

void OfflineSDFRenderer::destroyRenderContext() {
    VK_CHECK(vkDeviceWaitIdle(logicalDevice));
    if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
    }
    if (offscreenImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(logicalDevice, offscreenImageView, nullptr);
        offscreenImageView = VK_NULL_HANDLE;
    }
    if (offscreenImage != VK_NULL_HANDLE) {
        vkDestroyImage(logicalDevice, offscreenImage, nullptr);
        offscreenImage = VK_NULL_HANDLE;
    }
    if (offscreenImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(logicalDevice, offscreenImageMemory, nullptr);
        offscreenImageMemory = VK_NULL_HANDLE;
    }
}

void OfflineSDFRenderer::destroy() {
    VK_CHECK(vkDeviceWaitIdle(logicalDevice));
    vkutils::destroyFences(logicalDevice, fences);
    destroyPipeline();
    destroyRenderContext();
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
    if (queryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(logicalDevice, queryPool, nullptr);
        queryPool = VK_NULL_HANDLE;
    }
    if (vertShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
        vertShaderModule = VK_NULL_HANDLE;
    }
    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }
    if (logicalDevice != VK_NULL_HANDLE) {
        vkDestroyDevice(logicalDevice, nullptr);
        logicalDevice = VK_NULL_HANDLE;
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}
