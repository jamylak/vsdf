#include "sdf_renderer.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

namespace {
spdlog::level::level_enum parseLogLevel(const std::string &levelStr) {
    static const std::unordered_map<std::string, spdlog::level::level_enum>
        kLevels = {{"trace", spdlog::level::trace},
                   {"debug", spdlog::level::debug},
                   {"info", spdlog::level::info},
                   {"warn", spdlog::level::warn},
                   {"error", spdlog::level::err},
                   {"critical", spdlog::level::critical},
                   {"off", spdlog::level::off}};

    std::string normalized(levelStr);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const auto it = kLevels.find(normalized);
    if (it == kLevels.end()) {
        throw std::runtime_error("Invalid log level: " + levelStr);
    }
    return it->second;
}
} // namespace

int main(int argc, char **argv) {
    bool useToyTemplate = false;
    std::optional<uint32_t> maxFrames;
    bool headless = false;
    auto logLevel = spdlog::level::info;
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
                throw std::runtime_error("--frames requires a positive integer value");
            }
            try {
                maxFrames = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument& e) {
                throw std::runtime_error("--frames requires a valid positive integer value");
            } catch (const std::out_of_range& e) {
                throw std::runtime_error("--frames value is out of range for a positive integer");
            }
            continue;
        } else if (arg == "--log-level") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--log-level requires a value (trace|debug|info|warn|error|critical|off)");
            }
            logLevel = parseLogLevel(argv[++i]);
            continue;
        } else if (arg.substr(0, 2) != "--") {
            if (shaderFile.empty()) {
                shaderFile = arg;
            }
            continue;
        } else {
            throw std::runtime_error("Unknown flag: " + arg);
        }
    }

    if (!std::filesystem::exists(shaderFile))
        throw std::runtime_error("Shader file does not exist: " +
                                 shaderFile.string());
    if (shaderFile.extension() != ".frag")
        throw std::runtime_error("Shader file is not a .frag file: " +
                                 shaderFile.string());

    spdlog::set_level(logLevel);
    spdlog::info("Setting things up...");
    spdlog::default_logger()->set_pattern("[%H:%M:%S] [%l] %v");

    SDFRenderer renderer{shaderFile, useToyTemplate, maxFrames, headless};
    renderer.setup();
    renderer.gameLoop();
    return 0;
}
