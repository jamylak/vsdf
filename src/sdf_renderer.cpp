#include "sdf_renderer.h"
#include "image_dump.h"
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

SDFRenderer::SDFRenderer(
    const std::string &fragShaderPath, bool useToyTemplate,
    std::optional<uint32_t> maxFrames,
    std::optional<std::filesystem::path> debugDumpPPMDir)
    : fragShaderPath(fragShaderPath), useToyTemplate(useToyTemplate),
      maxFrames(maxFrames), debugDumpPPMDir(debugDumpPPMDir) {}

void SDFRenderer::logDeviceLimits() const {
    spdlog::info("Device limits {:.3f}",
                 deviceProperties.limits.timestampPeriod);
}

void SDFRenderer::initDeviceQueue() {
    vkGetDeviceQueue(logicalDevice, graphicsQueueIndex, 0, &queue);
}

void SDFRenderer::createPipelineLayoutCommon() {
    pipelineLayout = vkutils::createPipelineLayout(logicalDevice);
}

void SDFRenderer::dumpDebugFrame(const ReadbackFrame &frame) {
    if (!debugDumpPPMDir) {
        return;
    }
    std::filesystem::create_directories(*debugDumpPPMDir);
    std::filesystem::path outPath =
        *debugDumpPPMDir / fmt::format("frame_{:04}.ppm", dumpedFrames);
    image_dump::writePPM(frame, outPath);
    dumpedFrames++;
}

void SDFRenderer::destroyPipelineCommon() noexcept {
    vkDestroyPipeline(logicalDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
}
