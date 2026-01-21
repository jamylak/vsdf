#ifndef IMAGE_DUMP_H
#define IMAGE_DUMP_H

#include "readback_frame.h"
#include <filesystem>

namespace image_dump {
void writePPM(const PPMDebugFrame &frame, const std::filesystem::path &path);
} // namespace image_dump

#endif // IMAGE_DUMP_H
