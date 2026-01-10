#ifndef FRAME_H
#define FRAME_H

#include <cstdint>
#include <vector>

struct Frame {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    std::vector<uint8_t> rgba;

    void allocateRGBA(uint32_t w, uint32_t h) {
        width = w;
        height = h;
        stride = w * 4;
        rgba.resize(static_cast<size_t>(stride) * static_cast<size_t>(height));
    }
};

#endif // FRAME_H
