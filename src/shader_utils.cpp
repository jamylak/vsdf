#include <cstddef>
#include <filesystem>
#include <fstream>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/SPIRV/Logger.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static constexpr char FRAG_SHADER_TEMPLATE[] = "shaders/toytemplate.frag";
static constexpr char FULLSCREEN_QUAD_VERT_SOURCE[] = R"(#version 450

layout(location = 0) out vec2 texCoord;

const vec2 vertices[6] = vec2[](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
);

void main() {
    uint index = gl_VertexIndex % 6;  // Ensure the index wraps around if needed
    gl_Position = vec4(vertices[index], 0.0, 1.0);
    texCoord = vertices[index] * 0.5 + 0.5;
}
)";

EShLanguage getShaderLang(const std::string &extension) {
    if (extension.length() < 5)
        throw std::runtime_error("Invalid shader extension: " + extension);

    switch (extension[1]) {
    case 'v': // .vert
        return EShLangVertex;
    case 't':
        switch (extension[3]) {
        case 'c':
            return EShLangTessControl; // .tesc
        case 'e':
            return EShLangTessEvaluation; // .tese
        default:
            throw std::runtime_error("Unsupported shader extension: " +
                                     extension);
        }
    case 'g':
        return EShLangGeometry; // .geom
    case 'f':
        [[likely]] return EShLangFragment; // .frag
    case 'c':
        return EShLangCompute; // .comp
    default:
        break;
    }

    throw std::runtime_error("Unsupported shader extension: " + extension);
}

namespace shader_utils {
// Read shader source code from file
std::string readShaderSource(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        spdlog::error("Failed to open shader source file: {}", filename);
        return "";
    }

    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0);

    std::string result;
    result.resize(static_cast<size_t>(size));
    file.read(result.data(), size);

    return result;
}

// Apply our template to the shader which includes things
// like iTime, iMouse and redefined main to allow
// running shadertoy shaders
std::string readShaderSourceWithTemplate(const std::string &templateFilename,
                                         const std::string &userFilename) {
    std::ifstream templateFile(templateFilename);
    std::ifstream userFile(userFilename);

    if (!templateFile.is_open()) {
        spdlog::error("Failed to open template shader source file: {}",
                      templateFilename);
        return "";
    }

    if (!userFile.is_open()) {
        spdlog::error("Failed to open user shader source file: {}",
                      userFilename);
        return "";
    }

    std::stringstream shaderSourceStream;
    shaderSourceStream << templateFile.rdbuf() << '\n' << userFile.rdbuf();

    return shaderSourceStream.str();
}

std::filesystem::path compile(const std::string &shaderFilename,
                              bool useToyTemplate = false) {
    // Used to compile .frag or .vert shaders
    // With .frag shaders we sometimes use the toy template
    // which does old school GLSL ShaderToy style format
    spdlog::info("Compiling shader: {}", shaderFilename);
    std::string shaderString;
    const char *shaderSource;
    std::string shaderExtension =
        std::filesystem::path(shaderFilename).extension().string();
    EShLanguage lang = getShaderLang(shaderExtension);
    if (useToyTemplate) {
        spdlog::debug("Using template for fragment shader {}", shaderFilename);
        shaderString =
            readShaderSourceWithTemplate(FRAG_SHADER_TEMPLATE, shaderFilename);
    } else {
        shaderString = readShaderSource(shaderFilename);
    }
    shaderSource = shaderString.data();

    glslang::InitializeProcess();
    glslang::TShader shader(lang);

    // https://github.com/KhronosGroup/glslang/blob/main/StandAlone/StandAlone.cpp#L588
    glslang::EShClient Client;
    glslang::EshTargetClientVersion ClientVersion;
    if (useToyTemplate) {
        spdlog::debug("FOR OPENGL");
        Client = glslang::EShClientOpenGL;
        ClientVersion = glslang::EShTargetOpenGL_450;
    } else {
        Client = glslang::EShClientVulkan;
        ClientVersion = glslang::EShTargetVulkan_1_0;
    }

    // https://github.com/KhronosGroup/glslang/blob/main/StandAlone/StandAlone.cpp#L1097
    glslang::EShTargetLanguage TargetLanguage = glslang::EShTargetSpv;
    glslang::EShTargetLanguageVersion TargetVersion = glslang::EShTargetSpv_1_0;
    shader.setEnvClient(Client, ClientVersion);
    shader.setEnvTarget(TargetLanguage, TargetVersion);

    shader.setStrings(&shaderSource, 1);

    bool result = shader.parse(GetDefaultResources(), 100, ENoProfile, false,
                               false, EShMsgVulkanRules);
    spdlog::info("Shader parsed: {}", result);
    spdlog::info("Shader info log: {}", shader.getInfoLog());
    spdlog::debug("Shader source: {}", shaderSource);

    glslang::TProgram program;
    program.addShader(&shader);
    bool linked = program.link(EShMsgDefault);
    spdlog::info("Shader linked: {}", linked);
    spdlog::info("Program info log: {}", program.getInfoLog());

    if (!linked) {
        spdlog::error("Failed to link shader program");
        throw std::runtime_error("Failed to link shader program");
    }

    // Now save SPIR-V binary to a file
    std::vector<unsigned int> spirv;
    spv::SpvBuildLogger logger{};
    glslang::GlslangToSpv(*program.getIntermediate(lang), spirv, &logger,
                          nullptr);

    spdlog::info("Logger messages: {}", logger.getAllMessages());

    // Save to file
    std::filesystem::path outputPath = shaderFilename;
    outputPath.replace_extension(".spv");
    // Convert to string to ensure char* path (Windows uses wchar_t* for paths)
    glslang::OutputSpvBin(spirv, outputPath.string().c_str());

    // Print some spirv as a test
    for (size_t i = 0; i < 3; i++) {
        spdlog::debug("spirv[{}]: {}", i, spirv[i]);
    }

    glslang::FinalizeProcess();
    return outputPath;
}
} // namespace shader_utils
