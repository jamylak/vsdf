#ifndef FFMPEG_ENCODE_SETTINGS_H
#define FFMPEG_ENCODE_SETTINGS_H

#include <string>

namespace ffmpeg_utils {
struct EncodeSettings {
    std::string outputPath;
    std::string codec = "libx264";
    int fps = 30;
    int crf = 20;
    std::string preset = "slow";
};
} // namespace ffmpeg_utils

#endif // FFMPEG_ENCODE_SETTINGS_H
