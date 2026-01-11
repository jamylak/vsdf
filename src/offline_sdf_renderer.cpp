#include "offline_sdf_renderer.h"
#include "ffmpeg_encoder.h"
#include "shader_utils.h"
#include "vkutils.h"
#include <cstddef>
#include <cstdint>
#include <spdlog/spdlog.h>
#include <stdexcept>

OfflineSDFRenderer::OfflineSDFRenderer(
    const std::string &fragShaderPath, uint32_t maxFrames,
    bool useToyTemplate,
    std::optional<std::filesystem::path> debugDumpPPMDir, uint32_t width,
    uint32_t height, uint32_t ringSize,
    ffmpeg_utils::EncodeSettings encodeSettings)
    : SDFRenderer(fragShaderPath, useToyTemplate, maxFrames, debugDumpPPMDir),
      imageSize({width, height}), ringSize(validateRingSize(ringSize)),
      encodeSettings(std::move(encodeSettings)) {}

uint32_t OfflineSDFRenderer::validateRingSize(uint32_t value) {
    if (value == 0 || value > MAX_FRAME_SLOTS) {
        throw std::runtime_error("ringSize must be 1..MAX_FRAME_SLOTS");
    }
    return value;
}

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
    const auto formatInfo = vkutils::getReadbackFormatInfo(imageFormat);
    readbackFormatInfo = formatInfo;
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

    for (uint32_t i = 0; i < ringSize; ++i) {
        RingSlot &slot = ringSlots[i];
        VK_CHECK(vkCreateImage(logicalDevice, &imageCreateInfo, nullptr,
                               &slot.image));

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(logicalDevice, slot.image,
                                     &memRequirements);

        VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = vkutils::findMemoryTypeIndex(
                physicalDevice, memRequirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };

        VK_CHECK(vkAllocateMemory(logicalDevice, &allocInfo, nullptr,
                                  &slot.imageMemory));
        VK_CHECK(
            vkBindImageMemory(logicalDevice, slot.image, slot.imageMemory, 0));

        imageViewCreateInfoTemplate.image = slot.image;
        VK_CHECK(vkCreateImageView(logicalDevice, &imageViewCreateInfoTemplate,
                                   nullptr, &slot.imageView));

        framebufferInfoTemplate.pAttachments = &slot.imageView;
        VK_CHECK(vkCreateFramebuffer(logicalDevice, &framebufferInfoTemplate,
                                     nullptr, &slot.framebuffer));

        slot.stagingBuffer = vkutils::createReadbackBuffer(
            logicalDevice, physicalDevice, imageBytes,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        slot.rowStride = imageSize.width * formatInfo.bytesPerPixel;

        VK_CHECK(vkMapMemory(logicalDevice, slot.stagingBuffer.memory, 0,
                             imageBytes, 0, &slot.mappedData));

        transitionImageLayout(slot.image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    if (queryPool == VK_NULL_HANDLE) {
        queryPool = vkutils::createQueryPool(logicalDevice, ringSize);
    }
    if (fences.count == 0) {
        fences = vkutils::createFences(logicalDevice, ringSize);
    }
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
        vkutils::createCommandBuffers(logicalDevice, commandPool, ringSize);
}

void OfflineSDFRenderer::recordCommandBuffer(uint32_t slotIndex,
                                             uint32_t currentFrame) {
    RingSlot &slot = ringSlots[slotIndex];
    VkCommandBuffer commandBuffer = commandBuffers.commandBuffers[slotIndex];
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    VkRenderPassBeginInfo renderPassBeginInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = slot.framebuffer,
        .renderArea = {{0, 0}, imageSize},
        .clearValueCount = 0,
    };

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    vkCmdResetQueryPool(commandBuffer, queryPool, slotIndex * 2, 2);
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        queryPool, slotIndex * 2);
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    const vkutils::PushConstants pushConstants = getPushConstants(currentFrame);
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(vkutils::PushConstants), &pushConstants);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = {imageSize.width, imageSize.height},
    };

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(imageSize.width),
        .height = static_cast<float>(imageSize.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        queryPool, slotIndex * 2 + 1);
    vkCmdEndRenderPass(commandBuffer);

    // Transition image layout to TRANSFER_SRC_OPTIMAL so we can
    // copy it to the staging buffer.
    VkImageMemoryBarrier barrierToTransfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = slot.image,
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

    vkCmdCopyImageToBuffer(commandBuffer, slot.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           slot.stagingBuffer.buffer, 1, &region);

    // Transition image back to COLOR_ATTACHMENT_OPTIMAL
    // for next frame render.
    VkImageMemoryBarrier barrierToColor{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = slot.image,
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
}

void OfflineSDFRenderer::transitionImageLayout(VkImage image,
                                               VkImageLayout oldLayout,
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
        .image = image,
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

ReadbackFrame OfflineSDFRenderer::debugReadbackOffscreenImage(const RingSlot &slot) {
    const auto formatInfo = readbackFormatInfo;
    const uint8_t *data = static_cast<const uint8_t *>(slot.mappedData);

    ReadbackFrame frame;
    frame.allocateRGB(imageSize.width, imageSize.height);
    const uint8_t *src = data;
    const size_t pixelCount = static_cast<size_t>(imageSize.width) *
                              static_cast<size_t>(imageSize.height);
    for (size_t i = 0; i < pixelCount; ++i) {
        const size_t srcOffset = i * formatInfo.bytesPerPixel;
        const size_t dstOffset = i * 3;
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
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

    return frame;
}

void OfflineSDFRenderer::renderFrames() {
    // Default to 1 frame for now...
    // TODO: Check if best to instead make maxFrames required
    // when doing offline render???
    uint32_t totalFrames = maxFrames;
    startEncoding();
    for (uint32_t currentFrame = 0; currentFrame < totalFrames;
         ++currentFrame) {
        const uint32_t slotIndex = currentFrame % ringSize;
        waitForSlotEncode(slotIndex);

        VK_CHECK(vkResetFences(logicalDevice, 1, &fences.fences[slotIndex]));
        recordCommandBuffer(slotIndex, currentFrame);

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers.commandBuffers[slotIndex],
        };
        VK_CHECK(
            vkQueueSubmit(queue, 1, &submitInfo, fences.fences[slotIndex]));
        enqueueEncode(slotIndex, currentFrame);
    }

    // Finalise after the for loop finished
    stopEncoding();

    spdlog::info("Offline render done.");
    destroy();
}

void OfflineSDFRenderer::startEncoding() {
    const AVPixelFormat srcFormat =
        readbackFormatInfo.swapRB ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGBA;
    const int srcStride =
        static_cast<int>(imageSize.width * readbackFormatInfo.bytesPerPixel);

    encodeStop = false;
    encodeFailed = false;

    encoder = std::make_unique<ffmpeg_utils::FfmpegEncoder>(
        encodeSettings, static_cast<int>(imageSize.width),
        static_cast<int>(imageSize.height), srcFormat, srcStride);
    encoder->open();

    encoderThread = std::thread([this]() {
        try {
            while (true) {
                EncodeItem item;
                {
                    std::unique_lock<std::mutex> lock(encodeMutex);
                    encodeCv.wait(lock, [this]() {
                        return encodeStop || !encodeQueue.empty();
                    });
                    if (encodeQueue.empty()) {
                        if (encodeStop)
                            break;
                        continue;
                    }
                    item = encodeQueue.front();
                    encodeQueue.pop_front();
                    encodeCv.notify_all();
                }

                RingSlot &slot = ringSlots[item.slotIndex];
                VK_CHECK(vkWaitForFences(logicalDevice, 1,
                                         &fences.fences[item.slotIndex],
                                         VK_TRUE, UINT64_MAX));

                if (debugDumpPPMDir) {
                    // Blocking readback + PPM dump; this will stall the encode
                    // thread but remains an optional debug extra.
                    ReadbackFrame frame = debugReadbackOffscreenImage(slot);
                    dumpDebugFrame(frame);
                }

                const uint8_t *src =
                    static_cast<const uint8_t *>(slot.mappedData);
                encoder->encodeFrame(src, item.frameIndex);

                {
                    std::lock_guard<std::mutex> lock(encodeMutex);
                    slot.pendingEncode = false;
                }
                encodeCv.notify_all();
            }

            encoder->flush();
        } catch (const std::exception &e) {
            spdlog::error("FFmpeg encode thread failed: {}", e.what());
            {
                std::lock_guard<std::mutex> lock(encodeMutex);
                encodeFailed = true;
                encodeStop = true;
                encodeQueue.clear();
                for (size_t i = 0; i < ringSize; ++i) {
                    ringSlots[i].pendingEncode = false;
                }
            }
            encodeCv.notify_all();
        }
    });
}

void OfflineSDFRenderer::stopEncoding() {
    {
        std::lock_guard<std::mutex> lock(encodeMutex);
        encodeStop = true;
    }
    encodeCv.notify_all();
    if (encoderThread.joinable()) {
        encoderThread.join();
    }
    encoder.reset();
}

void OfflineSDFRenderer::enqueueEncode(uint32_t slotIndex,
                                       uint32_t frameIndex) {
    std::unique_lock<std::mutex> lock(encodeMutex);
    if (encodeFailed) {
        throw std::runtime_error("FFmpeg encoder failed");
    }
    encodeCv.wait(lock,
                  [this]() { return encodeQueue.size() < ringSize; });
    if (encodeFailed) {
        throw std::runtime_error("FFmpeg encoder failed");
    }
    RingSlot &slot = ringSlots[slotIndex];
    slot.pendingEncode = true;
    encodeQueue.push_back(EncodeItem{slotIndex, frameIndex});
    lock.unlock();
    encodeCv.notify_all();
}

void OfflineSDFRenderer::waitForSlotEncode(uint32_t slotIndex) {
    std::unique_lock<std::mutex> lock(encodeMutex);
    encodeCv.wait(lock, [this, slotIndex]() {
        return encodeFailed || !ringSlots[slotIndex].pendingEncode;
    });
    if (encodeFailed)
        throw std::runtime_error("FFmpeg encoder failed");
}

void OfflineSDFRenderer::destroyPipeline() {
    vkDestroyPipeline(logicalDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
}

void OfflineSDFRenderer::destroyRenderContext() {
    VK_CHECK(vkDeviceWaitIdle(logicalDevice));
    for (size_t i = 0; i < ringSize; ++i) {
        RingSlot &slot = ringSlots[i];
        if (slot.framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(logicalDevice, slot.framebuffer, nullptr);
            slot.framebuffer = VK_NULL_HANDLE;
        }
        if (slot.imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(logicalDevice, slot.imageView, nullptr);
            slot.imageView = VK_NULL_HANDLE;
        }
        if (slot.image != VK_NULL_HANDLE) {
            vkDestroyImage(logicalDevice, slot.image, nullptr);
            slot.image = VK_NULL_HANDLE;
        }
        if (slot.imageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(logicalDevice, slot.imageMemory, nullptr);
            slot.imageMemory = VK_NULL_HANDLE;
        }
        if (slot.stagingBuffer.buffer != VK_NULL_HANDLE ||
            slot.stagingBuffer.memory != VK_NULL_HANDLE) {
            if (slot.mappedData) {
                vkUnmapMemory(logicalDevice, slot.stagingBuffer.memory);
                slot.mappedData = nullptr;
            }
            vkutils::destroyReadbackBuffer(logicalDevice, slot.stagingBuffer);
        }
        slot.pendingReadback = false;
        slot.pendingEncode = false;
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
