#include "readback_frame.h"

#include <gtest/gtest.h>

TEST(Frame, BufferSizeMatchesDimensions) {
    PPMDebugFrame frame{};
    frame.allocateRGB(64, 32);

    EXPECT_EQ(frame.rgb.size(),
              static_cast<size_t>(frame.height) *
                  static_cast<size_t>(frame.stride));
}
