#include "sdf_renderer.h"
#include <filesystem>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

int main(int argc, char **argv) {
    bool useToyTemplate = false;
    std::optional<uint32_t> maxFrames;
    bool headless = false;
    std::filesystem::path shaderFile;

    if (argc < 2)
        throw std::runtime_error("No shader file provided.");

    std::string arg;
    for (int i = 1; i < argc; ++i) {
        arg = argv[i];
        if (arg == "--toy") {
            useToyTemplate = true;
            continue;
        } else if (arg == "--headless") {
            headless = true;
            continue;
        } else if (arg == "--frames") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--frames requires a value");
            }
            maxFrames = static_cast<uint32_t>(std::stoul(argv[++i]));
            continue;
        } else if (arg.substr(0, 2) !=
                   "--") { // Assuming shader file is not preceded by "--"
            shaderFile = arg;
            break;
        }
    }

    if (!std::filesystem::exists(shaderFile))
        throw std::runtime_error("Shader file does not exist: " +
                                 shaderFile.string());
    if (shaderFile.extension() != ".frag")
        throw std::runtime_error("Shader file is not a .frag file: " +
                                 shaderFile.string());

    spdlog::set_level(spdlog::level::info);
    spdlog::info("Setting things up...");
    spdlog::default_logger()->set_pattern("[%H:%M:%S] [%l] %v");

    SDFRenderer renderer{shaderFile, useToyTemplate, maxFrames, headless};
    renderer.setup();
    renderer.gameLoop();
    return 0;
}
