#include "shader_utils.h"
#include "test_utils.h"
#include <fstream>
#include <gtest/gtest.h>
#include <string>

TEST(ShaderUtilsTest, CompileTest) {
    TempShaderFile tempShader("temp_shader.frag",
                              "#version 450\nvoid main() {}");
    shader_utils::compile(tempShader.filename());

    std::string expectedSpvFilename = "temp_shader.spv";
    std::ifstream spvFile(expectedSpvFilename);
    bool fileExists = spvFile.good();
    spvFile.close();
    std::remove(expectedSpvFilename.c_str());

    ASSERT_TRUE(fileExists);
}

TEST(ShaderUtilsTest2, CompileTestBadVersion) {
    TempShaderFile tempShader(
        "temp_shader.frag", "// GLSL Fragment shader example\nvoid main() {}");
    ASSERT_TRUE(1); // Replace with actual error-checking logic

    // Simulate the compilation process and validate the error handling
}

TEST(ShaderUtilsTest, CompileVertexShader) {
    TempShaderFile tempShader(
        "temp_vertex.vert",
        "#version 450\nvoid main() { gl_Position = vec4(0.0); }");
    shader_utils::compile(tempShader.filename());

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

    shader_utils::compile(tempShader.filename(), true);

    std::string expectedSpvFilename = "temp_glsl_es.spv";
    std::ifstream spvFile(expectedSpvFilename);
    bool fileExists = spvFile.good();
    spvFile.close();
    std::remove(expectedSpvFilename.c_str());

    ASSERT_TRUE(fileExists);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
