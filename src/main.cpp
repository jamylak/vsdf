#include "online_sdf_renderer.h"
#include "ffmpeg_encode_settings.h"
#if defined(VSDF_ENABLE_FFMPEG)
#include "offline_sdf_renderer.h"
#endif
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

namespace {
constexpr const char kVersion[] = "vsdf dev";

void printHelp(const char *exe) {
    fmt::print("Usage: {} [options] <shader.frag>\nExample: {} --toy "
               "shaders/testtoyshader.frag\n",
               exe, exe);
}

void printVersion() { fmt::print("{}\n", kVersion); }

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
    std::optional<std::filesystem::path> debugDumpPPMDir;
#if defined(VSDF_ENABLE_FFMPEG)
    uint32_t offlineRingSize = OFFSCREEN_DEFAULT_RING_SIZE;
    uint32_t offlineWidth = OFFSCREEN_DEFAULT_WIDTH;
    uint32_t offlineHeight = OFFSCREEN_DEFAULT_HEIGHT;
    ffmpeg_utils::EncodeSettings encodeSettings{};
#endif
    auto logLevel = spdlog::level::info;
    std::filesystem::path shaderFile;

    if (argc < 2)
        throw std::runtime_error("No shader file provided.");

    std::string arg;
    for (int i = 1; i < argc; ++i) {
        arg = argv[i];
        if (arg == "--help") {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "--version") {
            printVersion();
            return 0;
        } else if (arg == "--toy") {
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
            } catch (const std::invalid_argument&) {
                throw std::runtime_error("--frames requires a valid positive integer value");
            } catch (const std::out_of_range&) {
                throw std::runtime_error("--frames value is out of range for a positive integer");
            }
            continue;
        } else if (arg == "--log-level") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--log-level requires a value (trace|debug|info|warn|error|critical|off)");
            }
            logLevel = parseLogLevel(argv[++i]);
            continue;
        } else if (arg == "--debug-dump-ppm") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--debug-dump-ppm requires a directory path");
            }
            debugDumpPPMDir = argv[++i];
            continue;
        }

#if defined(VSDF_ENABLE_FFMPEG)
        if (arg == "--ffmpeg-width") {
            if (i + 1 >= argc) {
                throw std::runtime_error(
                    "--ffmpeg-width requires a positive integer value");
            }
            try {
                offlineWidth = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument &) {
                throw std::runtime_error(
                    "--ffmpeg-width requires a valid positive integer value");
            } catch (const std::out_of_range &) {
                throw std::runtime_error(
                    "--ffmpeg-width value is out of range for a positive integer");
            }
            if (offlineWidth == 0) {
                throw std::runtime_error(
                    "--ffmpeg-width requires a positive integer value");
            }
            continue;
        } else if (arg == "--ffmpeg-height") {
            if (i + 1 >= argc) {
                throw std::runtime_error(
                    "--ffmpeg-height requires a positive integer value");
            }
            try {
                offlineHeight = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument &) {
                throw std::runtime_error(
                    "--ffmpeg-height requires a valid positive integer value");
            } catch (const std::out_of_range &) {
                throw std::runtime_error(
                    "--ffmpeg-height value is out of range for a positive integer");
            }
            if (offlineHeight == 0) {
                throw std::runtime_error(
                    "--ffmpeg-height requires a positive integer value");
            }
            continue;
        } else if (arg == "--ffmpeg-ring-buffer-size") {
            if (i + 1 >= argc) {
                throw std::runtime_error(
                    "--ffmpeg-ring-buffer-size requires a positive integer value");
            }
            try {
                offlineRingSize =
                    static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument &) {
                throw std::runtime_error(
                    "--ffmpeg-ring-buffer-size requires a valid positive integer value");
            } catch (const std::out_of_range &) {
                throw std::runtime_error(
                    "--ffmpeg-ring-buffer-size value is out of range for a positive integer");
            }
            if (offlineRingSize == 0) {
                throw std::runtime_error(
                    "--ffmpeg-ring-buffer-size requires a positive integer value");
            }
            continue;
        } else if (arg == "--ffmpeg-output" || arg == "--output-path") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--ffmpeg-output requires a file path");
            }
            encodeSettings.outputPath = argv[++i];
            continue;
        } else if (arg == "--ffmpeg-fps") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--ffmpeg-fps requires a positive integer value");
            }
            try {
                encodeSettings.fps = std::stoi(argv[++i]);
            } catch (const std::exception &) {
                throw std::runtime_error("--ffmpeg-fps requires a valid integer value");
            }
            if (encodeSettings.fps <= 0) {
                throw std::runtime_error("--ffmpeg-fps requires a positive integer value");
            }
            continue;
        } else if (arg == "--ffmpeg-codec") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--ffmpeg-codec requires a codec name");
            }
            encodeSettings.codec = argv[++i];
            continue;
        } else if (arg == "--ffmpeg-crf") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--ffmpeg-crf requires an integer value");
            }
            try {
                encodeSettings.crf = std::stoi(argv[++i]);
            } catch (const std::exception &) {
                throw std::runtime_error("--ffmpeg-crf requires a valid integer value");
            }
            continue;
        } else if (arg == "--ffmpeg-preset") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--ffmpeg-preset requires a value");
            }
            encodeSettings.preset = argv[++i];
            continue;
        }
#endif

        if (arg.substr(0, 2) != "--") {
            if (shaderFile.empty()) {
                shaderFile = arg;
            }
            continue;
        }
        throw std::runtime_error("Unknown flag: " + arg);
    }

    if (!std::filesystem::exists(shaderFile))
        throw std::runtime_error("Shader file does not exist: " +
                                 shaderFile.string());
    if (shaderFile.extension() != ".frag")
        throw std::runtime_error("Shader file is not a .frag file: " +
                                 shaderFile.string());

#if defined(VSDF_ENABLE_FFMPEG)
    const bool useFfmpeg = !encodeSettings.outputPath.empty();
    if (!encodeSettings.outputPath.empty() && !maxFrames) {
        throw std::runtime_error(
            "--frames must be set when using --ffmpeg-output");
    }
#endif

    spdlog::set_level(logLevel);
    spdlog::info("Setting things up...");
    spdlog::default_logger()->set_pattern("[%H:%M:%S] [%l] %v");

#if defined(VSDF_ENABLE_FFMPEG)
    if (useFfmpeg) {
        OfflineSDFRenderer renderer{shaderFile.string(), *maxFrames,
                                    useToyTemplate, debugDumpPPMDir,
                                    offlineWidth, offlineHeight,
                                    offlineRingSize, encodeSettings};
        renderer.setup();
        renderer.renderFrames();
    } else {
        OnlineSDFRenderer renderer{shaderFile.string(), useToyTemplate,
                                   maxFrames, headless, debugDumpPPMDir};
        renderer.setup();
        renderer.gameLoop();
    }
#else
    OnlineSDFRenderer renderer{shaderFile.string(), useToyTemplate,
                               maxFrames, headless, debugDumpPPMDir};
    renderer.setup();
    renderer.gameLoop();
#endif
    return 0;
}
