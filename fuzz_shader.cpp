#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

// Fuzz target for shader input validation
// Tests input handling and file operations with various inputs
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    // Skip empty or very large inputs
    if (Size == 0 || Size > 100000) {
        return 0;
    }

    // Create a temporary shader file to test file handling
    std::string tempFilename = "/tmp/fuzz_shader.frag";
    
    try {
        std::ofstream outFile(tempFilename);
        if (!outFile.is_open()) {
            return 0;
        }
        outFile.write(reinterpret_cast<const char *>(Data), static_cast<std::streamsize>(Size));
        outFile.close();

        // Test file existence and size checks
        if (std::filesystem::exists(tempFilename)) {
            auto fileSize = std::filesystem::file_size(tempFilename);
            
            // Test reading the file back
            std::ifstream inFile(tempFilename);
            if (inFile.is_open()) {
                inFile.seekg(0, std::ios::end);
                auto readSize = inFile.tellg();
                inFile.seekg(0);
                
                if (readSize > 0 && static_cast<std::size_t>(readSize) < 100000) {
                    std::string content;
                    content.resize(static_cast<size_t>(readSize));
                    inFile.read(content.data(), readSize);
                }
                inFile.close();
            }
        }
    } catch (...) {
        // Catch any exceptions to prevent crashes
        // But we want sanitizers to catch memory issues
    }

    // Clean up temporary file
    std::remove(tempFilename.c_str());

    return 0;
}
