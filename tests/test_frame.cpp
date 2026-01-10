#include "frame.h"

#include <gtest/gtest.h>

TEST(Frame, BufferSizeMatchesDimensions) {
    Frame frame{};
    frame.allocateRGBA(64, 32);

    EXPECT_EQ(frame.rgba.size(),
              static_cast<size_t>(frame.height) *
                  static_cast<size_t>(frame.stride));
}
