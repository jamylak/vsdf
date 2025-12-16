#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "shader_utils.h"

// LibFuzzer entry point for fuzzing shader compilation
// This function receives random data from the fuzzer and tests shader_utils::compile
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Ignore empty inputs
    if (size == 0) {
        return 0;
    }

    // Create a temporary file with the fuzzer data
    std::string tempFilename = "/tmp/fuzz_shader.frag";
    std::string spvFilename = "/tmp/fuzz_shader.spv";
    
    try {
        {
            std::ofstream tempFile(tempFilename, std::ios::binary);
            if (!tempFile.is_open()) {
                return 0;
            }
            
            tempFile.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        } // tempFile automatically closed here
        
        // Try to compile the shader without toy template
        // We expect many failures with malformed input, which is fine
        try {
            shader_utils::compile(tempFilename, false);
            std::filesystem::remove(spvFilename);
        } catch (const std::exception &e) {
            // Expected to throw on invalid input - this is normal
        }
        
        // Also test with toy template mode (recreate the file)
        {
            std::ofstream tempFile(tempFilename, std::ios::binary);
            if (tempFile.is_open()) {
                tempFile.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
            }
        }
        
        try {
            shader_utils::compile(tempFilename, true);
            std::filesystem::remove(spvFilename);
        } catch (const std::exception &e) {
            // Expected to throw on invalid input
        }
        
        // Clean up the temp shader file
        std::filesystem::remove(tempFilename);
        
    } catch (...) {
        // Clean up on any unexpected error
        std::filesystem::remove(tempFilename);
        std::filesystem::remove(spvFilename);
    }
    
    return 0;
}
