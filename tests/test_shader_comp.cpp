#include "shader_utils.h"
#include "test_utils.h"
#include <fstream>
#include <gtest/gtest.h>
#include <string>

TEST(ShaderUtilsTest, CompileTest) {
    TempShaderFile tempShader("temp_shader.frag",
                              "#version 450\nvoid main() {}");
    shader_utils::compileToPath(tempShader.filename());

    std::string expectedSpvFilename = "temp_shader.spv";
    std::ifstream spvFile(expectedSpvFilename);
    bool fileExists = spvFile.good();
    spvFile.close();
    std::remove(expectedSpvFilename.c_str());

    ASSERT_TRUE(fileExists);
}

TEST(ShaderUtilsTest, CompileTestBadVersion) {
    TempShaderFile tempShader(
        "temp_shader.frag", "// GLSL Fragment shader example\nvoid main() {}");
    ASSERT_THROW(shader_utils::compileToPath(tempShader.filename()),
                 std::runtime_error);
}

TEST(ShaderUtilsTest, CompileVertexShader) {
    TempShaderFile tempShader(
        "temp_vertex.vert",
        "#version 450\nvoid main() { gl_Position = vec4(0.0); }");
    shader_utils::compileToPath(tempShader.filename());

    std::string expectedSpvFilename = "temp_vertex.spv";
    std::ifstream spvFile(expectedSpvFilename);
    bool fileExists = spvFile.good();
    spvFile.close();
    std::remove(expectedSpvFilename.c_str());

    ASSERT_TRUE(fileExists);
}

TEST(ShaderUtilsTest, CompileGLSLESTest) {
    // Example of a simple GLSL ES shader like those used in Shadertoy
    // By passing true param here it should prepend the template
    // that will make this compatible with our push constants
    // so we do not need to specify version
    TempShaderFile tempShader(
        "temp_glsl_es.frag",
        "precision highp float;\n"
        "void mainImage( out vec4 fragColor, in vec2 fragCoord ){\n"
        "    fragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red\n"
        "}");

    shader_utils::compileToPath(tempShader.filename(), true);

    std::string expectedSpvFilename = "temp_glsl_es.spv";
    std::ifstream spvFile(expectedSpvFilename);
    bool fileExists = spvFile.good();
    spvFile.close();
    std::remove(expectedSpvFilename.c_str());

    ASSERT_TRUE(fileExists);
}

TEST(ShaderUtilsTest, CompileToyShaderTest) {
    // This shader should compile successfully
    shader_utils::compileToPath(SHADER_DIR "testtoyshader.frag", true);

    std::string expectedSpvFilename = std::string(SHADER_DIR) + "testtoyshader.spv";
    std::ifstream spvFile(expectedSpvFilename);
    bool fileExists = spvFile.good();
    spvFile.close();
    std::remove(expectedSpvFilename.c_str());

    ASSERT_TRUE(fileExists);
}

TEST(ShaderUtilsTest, CompileToyShaderFailTest) {
    // This shader is missing the mainImage function, so it should fail to
    // compile
    TempShaderFile tempShader("temp_shader_fail.frag",
                              "void main() {}");

    ASSERT_THROW(shader_utils::compileToPath(tempShader.filename(), true),
                 std::runtime_error);
}

TEST(ShaderUtilsTest, FileNotFoundTest) {
    ASSERT_THROW(shader_utils::compileToPath("non_existent_shader.frag"),
                 std::runtime_error);
}

TEST(ShaderUtilsTest, CompileEmptyFile) {
    TempShaderFile tempShader("empty.frag", "");
    ASSERT_THROW(shader_utils::compileToPath(tempShader.filename()),
                 std::runtime_error);
}

TEST(ShaderUtilsTest, CompileWithUnknownExtension) {
    TempShaderFile tempShader("shader.txt", "#version 450\nvoid main() {}");
    // glslc typically fails if it can't determine the shader stage from the
    // extension
    ASSERT_THROW(shader_utils::compileToPath(tempShader.filename()),
                 std::runtime_error);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
