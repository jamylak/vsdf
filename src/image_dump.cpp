#include "image_dump.h"

#include <fstream>
#include <stdexcept>

namespace image_dump {
void writePPM(const PPMDebugFrame &frame, const std::filesystem::path &path) {
    // Layout (RGB, row-major):
    // row 0: [R G B][R G B]...[R G B]  (width pixels)
    // row 1: [R G B][R G B]...[R G B]
    // ...
    // row (height-1)
    //
    // stride = width * 3 bytes per row
    // total bytes = height * stride
    if (frame.width == 0 || frame.height == 0 || frame.stride == 0) {
        throw std::runtime_error("Invalid frame dimensions for PPM dump");
    }
    if (frame.rgb.size() < static_cast<size_t>(frame.stride) *
                               static_cast<size_t>(frame.height)) {
        throw std::runtime_error("Frame buffer is smaller than expected");
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open PPM output: " +
                                 path.string());
    }

    out << "P6\n" << frame.width << " " << frame.height << "\n255\n";
    for (uint32_t y = 0; y < frame.height; ++y) {
        const uint8_t *row = frame.rgb.data() +
                             static_cast<size_t>(y) *
                                 static_cast<size_t>(frame.stride);
        out.write(reinterpret_cast<const char *>(row),
                  static_cast<std::streamsize>(frame.width) * 3);
    }
}
} // namespace image_dump
