#ifndef READBACK_FRAME_H
#define READBACK_FRAME_H

#include <cstdint>
#include <vector>

// CPU-side buffer produced by GPU readback (RGB, row-major).
// Just used for debugging through PPM dumps
struct ReadbackFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    std::vector<uint8_t> rgb;

    void allocateRGB(uint32_t w, uint32_t h) {
        width = w;
        height = h;
        stride = w * 3;
        rgb.resize(static_cast<size_t>(stride) * static_cast<size_t>(height));
    }
};

#endif // READBACK_FRAME_H
