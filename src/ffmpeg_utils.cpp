#include "ffmpeg_utils.h"

#include <spdlog/fmt/fmt.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/version.h>
}

namespace ffmpeg_utils {
std::string getLibavformatVersion() {
    unsigned ver = avformat_version();
    unsigned major = AV_VERSION_MAJOR(ver);
    unsigned minor = AV_VERSION_MINOR(ver);
    unsigned micro = AV_VERSION_MICRO(ver);
    return fmt::format("libavformat {}.{}.{}", major, minor, micro);
}
} // namespace ffmpeg_utils
