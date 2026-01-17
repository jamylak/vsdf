#ifndef SHADER_UTILS_H
#define SHADER_UTILS_H
#include <filesystem>
#include <string>
#include <vector>

namespace shader_utils {
// Take a shader file eg. planet.frag
// and produce SPIR-V in memory.
std::vector<uint32_t> compileFileToSpirv(const std::string &shaderFilename,
                                         bool useToyTemplate = false);

// Compile the embedded fullscreen quad vertex shader directly to SPIR-V.
std::vector<uint32_t> compileFullscreenQuadVertSpirv();
} // namespace shader_utils

#endif // SHADER_UTILS_H
