#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

[[nodiscard]] static std::vector<char>
loadBinaryFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    if (file.gcount() != fileSize) {
        throw std::runtime_error("Failed to read the complete file: " +
                                 filename);
    }

    return buffer;
};

#endif // FILEUTILS_H
