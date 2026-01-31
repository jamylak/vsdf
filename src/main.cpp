#include "ffmpeg_encode_settings.h"
#include "online_sdf_renderer.h"
#include "shader_templates.h"
#if defined(VSDF_ENABLE_FFMPEG)
#include "offline_sdf_renderer.h"
#endif
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

namespace {
constexpr const char kVersion[] = "vsdf dev";

class CLIError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

void printHelp(const char *exe) {
    fmt::print(
        "Usage: {} [options] <shader.frag>\n"
        "Example:\n"
        "  {} --new-toy example.frag\n"
        "  {} --toy example.frag\n\n"
        "Options:\n"
        "  --help                  Show this help message\n"
        "  --version               Show version information\n"
        "  --new-toy [name]        Create a new shader file with starter "
        "template.\n"
        "                          Prints the filename and exits.\n"
        "                          Generates random name like "
        "my_new_toy_12345.frag if not provided.\n"
        "  --template <name>       Template to use with --new-toy (default, "
        "plot)\n"
        "  --toy                   Use ShaderToy-style template wrapper\n"
        "  --no-focus              Don't steal window focus on startup and "
        "float\n"
        "  --headless              Hide the GLFW window (pair with xvfb-run in "
        "CI)\n"
        "  --frames <N>            Render N frames then exit\n"
        "  --log-level <trace|debug|info|warn|error|critical|off> Set spdlog "
        "verbosity (default: info)\n"
        "  --debug-dump-ppm <dir>  Copy the swapchain image before present "
        "(adds a stall); mainly for smoke tests or debugging\n"
        "  --ffmpeg-output <file>  Enable offline encoding; "
        "output file path (requires --frames)\n"
        "  --ffmpeg-fps <N>        Output FPS (default: 30)\n"
        "  --ffmpeg-crf <N>        Quality for libx264 (default: 20; lower is "
        "higher quality)\n"
        "  --ffmpeg-preset <name>  libx264 preset (default: slow)\n"
        "  --ffmpeg-codec <name>   FFmpeg codec (default: libx264)\n"
        "  --ffmpeg-width <N>      Output width (default: 1280)\n"
        "  --ffmpeg-height <N>     Output height (default: 720)\n"
        "  --ffmpeg-ring-buffer-size <N> Ring buffer size for offline render "
        "(default: 2)\n",
        exe, exe, exe);
}

void printVersion() {
#if defined(VSDF_ENABLE_FFMPEG)
    fmt::print("{}\n", kVersion);
#else
    fmt::print("{} [disable_ffmpeg]\n", kVersion);
#endif
}

std::filesystem::path createNewToyShader(const std::string &name,
                                         const std::string &templateName) {
    std::filesystem::path shaderPath;
    if (name.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(10000, 99999);
        shaderPath = fmt::format("my_new_toy_{}.frag", dis(gen));
    } else {
        shaderPath = name;
        if (shaderPath.extension() != ".frag") {
            shaderPath += ".frag";
        }
    }

    if (std::filesystem::exists(shaderPath)) {
        throw std::runtime_error(
            fmt::format("File already exists: {}", shaderPath.string()));
    }

    const char *templateBody;
    if (templateName == "plot") {
        templateBody = shader_templates::kPlotTemplate;
    } else {
        templateBody = shader_templates::kDefaultTemplate;
    }

    std::ofstream file(shaderPath);
    if (!file) {
        throw std::runtime_error(
            fmt::format("Failed to create file: {}", shaderPath.string()));
    }

    file << templateBody;
    file.close();

    fmt::print("{}\n", shaderPath.string());

    return shaderPath;
}

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
    std::transform(
        normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const auto it = kLevels.find(normalized);
    if (it == kLevels.end()) {
        throw std::runtime_error("Invalid log level: " + levelStr);
    }
    return it->second;
}
} // namespace

int run(int argc, char **argv) {
    bool useToyTemplate = false;
    std::optional<uint32_t> maxFrames;
    bool headless = false;
    bool noFocus = false;
    std::optional<std::filesystem::path> debugDumpPPMDir;
    std::optional<uint32_t> ciResizeAfter;
    std::optional<uint32_t> ciResizeWidth;
    std::optional<uint32_t> ciResizeHeight;
#if defined(VSDF_ENABLE_FFMPEG)
    uint32_t offlineRingSize = OFFSCREEN_DEFAULT_RING_SIZE;
    uint32_t offlineWidth = OFFSCREEN_DEFAULT_WIDTH;
    uint32_t offlineHeight = OFFSCREEN_DEFAULT_HEIGHT;
    ffmpeg_utils::EncodeSettings encodeSettings{};
#endif
    auto logLevel = spdlog::level::info;
    std::filesystem::path shaderFile;

    // Quick ways to make a new template
    // Exits and returns the template filename
    bool createNewToy = false;
    std::string newToyName;
    std::string templateName = "default";

    if (argc < 2)
        throw CLIError("No shader file provided.");

    std::string arg;
    for (int i = 1; i < argc; ++i) {
        arg = argv[i];
        if (arg == "--help") {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "--version") {
            printVersion();
            return 0;
        } else if (arg == "--new-toy") {
            createNewToy = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                newToyName = argv[++i];
            }
            continue;
        } else if (arg == "--template") {
            if (i + 1 >= argc) {
                throw CLIError(
                    "--template requires a template name (default, plot)");
            }
            templateName = argv[++i];
            continue;
        } else if (arg == "--toy") {
            useToyTemplate = true;
            continue;
        } else if (arg == "--no-focus") {
            noFocus = true;
            continue;
        } else if (arg == "--headless") {
            headless = true;
            continue;
        } else if (arg == "--frames") {
            if (i + 1 >= argc) {
                throw CLIError("--frames requires a positive integer value");
            }
            try {
                maxFrames = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument &) {
                throw CLIError(
                    "--frames requires a valid positive integer value");
            } catch (const std::out_of_range &) {
                throw CLIError(
                    "--frames value is out of range for a positive integer");
            }
            continue;
        } else if (arg == "--log-level") {
            if (i + 1 >= argc) {
                throw CLIError("--log-level requires a value "
                               "(trace|debug|info|warn|error|critical|off)");
            }
            logLevel = parseLogLevel(argv[++i]);
            continue;
        } else if (arg == "--debug-dump-ppm") {
            if (i + 1 >= argc) {
                throw CLIError("--debug-dump-ppm requires a directory path");
            }
            debugDumpPPMDir = argv[++i];
            continue;
        }

#if defined(VSDF_ENABLE_FFMPEG)
        if (arg == "--ffmpeg-width") {
            if (i + 1 >= argc) {
                throw CLIError(
                    "--ffmpeg-width requires a positive integer value");
            }
            try {
                offlineWidth = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument &) {
                throw CLIError(
                    "--ffmpeg-width requires a valid positive integer value");
            } catch (const std::out_of_range &) {
                throw CLIError("--ffmpeg-width value is out of range "
                               "for a positive integer");
            }
            if (offlineWidth == 0) {
                throw CLIError(
                    "--ffmpeg-width requires a positive integer value");
            }
            continue;
        } else if (arg == "--ffmpeg-height") {
            if (i + 1 >= argc) {
                throw CLIError(
                    "--ffmpeg-height requires a positive integer value");
            }
            try {
                offlineHeight = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument &) {
                throw CLIError(
                    "--ffmpeg-height requires a valid positive integer value");
            } catch (const std::out_of_range &) {
                throw CLIError("--ffmpeg-height value is out of "
                               "range for a positive integer");
            }
            if (offlineHeight == 0) {
                throw CLIError(
                    "--ffmpeg-height requires a positive integer value");
            }
            continue;
        } else if (arg == "--ffmpeg-ring-buffer-size") {
            if (i + 1 >= argc) {
                throw CLIError("--ffmpeg-ring-buffer-size requires a "
                               "positive integer value");
            }
            try {
                offlineRingSize = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument &) {
                throw CLIError("--ffmpeg-ring-buffer-size requires a "
                               "valid positive integer value");
            } catch (const std::out_of_range &) {
                throw CLIError("--ffmpeg-ring-buffer-size value is "
                               "out of range for a positive integer");
            }
            if (offlineRingSize == 0) {
                throw CLIError("--ffmpeg-ring-buffer-size requires a "
                               "positive integer value");
            }
            continue;
        } else if (arg == "--ffmpeg-output") {
            if (i + 1 >= argc) {
                throw CLIError("--ffmpeg-output requires a file path");
            }
            encodeSettings.outputPath = argv[++i];
            continue;
        } else if (arg == "--ffmpeg-fps") {
            if (i + 1 >= argc) {
                throw CLIError(
                    "--ffmpeg-fps requires a positive integer value");
            }
            try {
                encodeSettings.fps = std::stoi(argv[++i]);
            } catch (const std::exception &) {
                throw CLIError("--ffmpeg-fps requires a valid integer value");
            }
            if (encodeSettings.fps <= 0) {
                throw CLIError(
                    "--ffmpeg-fps requires a positive integer value");
            }
            continue;
        } else if (arg == "--ffmpeg-codec") {
            if (i + 1 >= argc) {
                throw CLIError("--ffmpeg-codec requires a codec name");
            }
            encodeSettings.codec = argv[++i];
            continue;
        } else if (arg == "--ffmpeg-crf") {
            if (i + 1 >= argc) {
                throw CLIError("--ffmpeg-crf requires an integer value");
            }
            try {
                encodeSettings.crf = std::stoi(argv[++i]);
            } catch (const std::exception &) {
                throw CLIError("--ffmpeg-crf requires a valid integer value");
            }
            continue;
        } else if (arg == "--ffmpeg-preset") {
            if (i + 1 >= argc) {
                throw CLIError("--ffmpeg-preset requires a value");
            }
            encodeSettings.preset = argv[++i];
            continue;
        }
#endif

        // CI-only flags (intentionally undocumented).
        if (arg == "--ci-resize-after") {
            if (i + 1 >= argc) {
                throw CLIError(
                    "--ci-resize-after requires a positive integer value");
            }
            try {
                ciResizeAfter = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument &) {
                throw CLIError(
                    "--ci-resize-after requires a valid positive integer value");
            } catch (const std::out_of_range &) {
                throw CLIError("--ci-resize-after value is out of range "
                               "for a positive integer");
            }
            continue;
        } else if (arg == "--ci-resize-width") {
            if (i + 1 >= argc) {
                throw CLIError(
                    "--ci-resize-width requires a positive integer value");
            }
            try {
                ciResizeWidth = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument &) {
                throw CLIError(
                    "--ci-resize-width requires a valid positive integer value");
            } catch (const std::out_of_range &) {
                throw CLIError("--ci-resize-width value is out of range "
                               "for a positive integer");
            }
            if (*ciResizeWidth == 0) {
                throw CLIError(
                    "--ci-resize-width requires a positive integer value");
            }
            continue;
        } else if (arg == "--ci-resize-height") {
            if (i + 1 >= argc) {
                throw CLIError(
                    "--ci-resize-height requires a positive integer value");
            }
            try {
                ciResizeHeight = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (const std::invalid_argument &) {
                throw CLIError(
                    "--ci-resize-height requires a valid positive integer value");
            } catch (const std::out_of_range &) {
                throw CLIError("--ci-resize-height value is out of range "
                               "for a positive integer");
            }
            if (*ciResizeHeight == 0) {
                throw CLIError(
                    "--ci-resize-height requires a positive integer value");
            }
            continue;
        }
        // END CI-only flags (intentionally undocumented).

        if (arg.substr(0, 2) != "--") {
            if (shaderFile.empty()) {
                shaderFile = arg;
            }
            continue;
        }
        throw CLIError("Unknown flag: " + arg);
    }

    if (createNewToy) {
        createNewToyShader(newToyName, templateName);
        return 0;
    }

    if (!std::filesystem::exists(shaderFile))
        throw CLIError("Shader file does not exist: " + shaderFile.string());
    if (shaderFile.extension() != ".frag")
        throw CLIError("Shader file is not a .frag file: " +
                       shaderFile.string());

#if defined(VSDF_ENABLE_FFMPEG)
    const bool useFfmpeg = !encodeSettings.outputPath.empty();
    if (!encodeSettings.outputPath.empty() && !maxFrames) {
        throw CLIError("--frames must be set when using --ffmpeg-output");
    }
#endif

    spdlog::set_level(logLevel);
    spdlog::info("Setting things up...");
    spdlog::default_logger()->set_pattern("[%H:%M:%S] [%l] %v");

    OnlineRenderOptions options{
        .maxFrames = maxFrames,
        .headless = headless,
        .noFocus = noFocus,
        .debugDumpPPMDir = debugDumpPPMDir,
        .ciResizeAfter = ciResizeAfter,
        .ciResizeWidth = ciResizeWidth,
        .ciResizeHeight = ciResizeHeight,
    };

    bool shouldRunOnline = true;
#if defined(VSDF_ENABLE_FFMPEG)
    if (useFfmpeg) {
        shouldRunOnline = false;
        OfflineRenderOptions offlineOptions{
            .maxFrames = *maxFrames,
            .debugDumpPPMDir = debugDumpPPMDir,
            .width = offlineWidth,
            .height = offlineHeight,
            .ringSize = offlineRingSize,
            .encodeSettings = encodeSettings,
        };
        OfflineSDFRenderer renderer{shaderFile.string(), useToyTemplate,
                                    std::move(offlineOptions)};
        renderer.setup();
        renderer.renderFrames();
    }
#endif
    if (shouldRunOnline) {
        OnlineSDFRenderer renderer{shaderFile.string(), useToyTemplate,
                                   options};
        renderer.setup();
        renderer.gameLoop();
    }
    return 0;
}

int main(int argc, char **argv) {
    try {
        return run(argc, argv);
    } catch (const CLIError &e) {
        fmt::print(stderr, "vsdf error: {}\n", e.what());
        printHelp(argv[0]);
        return 1;
    } catch (const std::exception &e) {
        fmt::print(stderr, "vsdf error: {}\n", e.what());
        return 1;
    } catch (...) {
        fmt::print(stderr, "vsdf error: unknown exception\n");
        return 1;
    }
}
