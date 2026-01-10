#ifndef IMAGE_DUMP_H
#define IMAGE_DUMP_H

#include "frame.h"
#include <filesystem>

namespace image_dump {
void writePPM(const Frame &frame, const std::filesystem::path &path);
} // namespace image_dump

#endif // IMAGE_DUMP_H
