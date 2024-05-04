#ifndef SHADER_UTILS_H
#define SHADER_UTILS_H
#include <filesystem>
#include <string>

namespace shader_utils {
// Take a shader file eg. planet.frag
// and produce planet.spv
std::filesystem::path compile(const std::string &shaderFilename,
                              bool useToyTemplate = false);
} // namespace shader_utils

#endif // SHADER_UTILS_H
