#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

[[nodiscard]] static std::vector<uint32_t>
loadBinaryFile(const std::string &filename) {

    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("Failed to open file: " + filename);

    std::streamoff fileSize = file.tellg();
    if (fileSize % static_cast<std::streamoff>(sizeof(uint32_t)) != 0)
        throw std::runtime_error("SPIR-V file size is not a multiple of 4: " +
                                 filename);

    std::size_t byteSize = static_cast<std::size_t>(fileSize);
    std::vector<uint32_t> buffer(byteSize / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()),
              static_cast<std::streamsize>(byteSize));

    if (file.gcount() != static_cast<std::streamsize>(byteSize))
        throw std::runtime_error("Failed to read the complete file: " +
                                 filename);

    return buffer;
};

#endif // FILEUTILS_H
