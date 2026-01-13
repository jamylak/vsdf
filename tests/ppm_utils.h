#ifndef PPM_UTILS_H
#define PPM_UTILS_H

#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ppm_utils {
constexpr uint32_t kPpmMaxValue = 255;

struct [[nodiscard]] PPMImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> data;
};

inline void skipWhitespaceAndComments(std::istream &in) {
    while (true) {
        int c = in.peek();
        if (c == EOF) {
            return;
        }
        if (c == '#') {
            std::string line;
            std::getline(in, line);
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            in.get();
            continue;
        }
        break;
    }
}

[[nodiscard]] inline PPMImage readPPM(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open PPM file");
    }

    std::string magic;
    in >> magic;
    if (magic != "P6") {
        throw std::runtime_error("PPM is not P6 format");
    }

    skipWhitespaceAndComments(in);
    uint32_t width = 0;
    uint32_t height = 0;
    in >> width >> height;

    skipWhitespaceAndComments(in);
    uint32_t maxval = 0;
    in >> maxval;
    if (maxval != kPpmMaxValue) {
        throw std::runtime_error("Unexpected PPM max value");
    }
    in.get();

    const size_t dataSize = static_cast<size_t>(width) *
                            static_cast<size_t>(height) * 3;
    std::vector<uint8_t> data(dataSize);
    in.read(reinterpret_cast<char *>(data.data()),
            static_cast<std::streamsize>(dataSize));
    if (in.gcount() != static_cast<std::streamsize>(dataSize)) {
        throw std::runtime_error("PPM data truncated");
    }

    return {width, height, std::move(data)};
}

[[nodiscard]] inline std::array<uint8_t, 3> pixelAt(const PPMImage &img,
                                                    uint32_t x, uint32_t y) {
    if (x >= img.width || y >= img.height) {
        throw std::out_of_range("pixelAt: coordinates out of bounds");
    }
    const size_t idx =
        (static_cast<size_t>(y) * img.width + x) * 3;
    return {img.data[idx + 0], img.data[idx + 1], img.data[idx + 2]};
}
} // namespace ppm_utils

#endif // PPM_UTILS_H
