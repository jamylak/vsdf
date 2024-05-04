#include "sdf_renderer.h"
#include <filesystem>
#include <spdlog/spdlog.h>

int main(int argc, char **argv) {
    bool useToyTemplate = false;
    std::filesystem::path shaderFile;

    if (argc < 2)
        throw std::runtime_error("No shader file provided.");

    std::string arg;
    for (int i = 1; i < argc; ++i) {
        arg = argv[i];
        if (arg == "--toy") {
            useToyTemplate = true;
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

    SDFRenderer renderer{shaderFile, useToyTemplate};
    renderer.setup();
    renderer.gameLoop();
    return 0;
}
