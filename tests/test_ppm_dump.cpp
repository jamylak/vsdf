#include <gtest/gtest.h>

#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr uint32_t kPpmMaxValue = 255;
struct [[nodiscard]] PPMImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> data;
};

void skipWhitespaceAndComments(std::istream &in) {
    while (true) {
        int c = in.peek();
        if (c == EOF) {
            // End of stream: nothing left to skip.
            return;
        }
        if (c == '#') {
            // PPM comment line: consume it.
            std::string line;
            std::getline(in, line);
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            // Skip whitespace between header tokens.
            in.get();
            continue;
        }
        break;
    }
}

[[nodiscard]] PPMImage readPPM(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open PPM file");
    }

    std::string magic;
    in >> magic;
    if (magic != "P6") {
        throw std::runtime_error("PPM is not P6 format");
    }

    // Sample PPM header (tokens may be separated by spaces/newlines):
    // P6\n
    // 640 480\n
    // 255\n
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
    in.get(); // consume single whitespace after header

    // width * height * RGB
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

[[nodiscard]] std::array<uint8_t, 3> pixelAt(const PPMImage &img, uint32_t x, uint32_t y) {
    if (x >= img.width || y >= img.height) {
        throw std::out_of_range("pixelAt: coordinates out of bounds");
    }
    const size_t idx =
        (static_cast<size_t>(y) * img.width + x) * 3; // RGB
    return {img.data[idx + 0], img.data[idx + 1], img.data[idx + 2]};
}
} // namespace

TEST(PPMDump, DebugQuadrants) {
    const auto outDir =
        std::filesystem::current_path() / "ppm_test_output";
    std::filesystem::create_directories(outDir);

    const auto shaderPath =
        std::filesystem::path(VSDF_SOURCE_DIR) / "shaders" /
        "debug_quadrants.frag";
    const auto oldCwd = std::filesystem::current_path();

    // Move to the current dir to resolve shaders correctly
    std::filesystem::current_path(VSDF_SOURCE_DIR);

    std::string cmd = std::string("\"") + VSDF_BINARY_PATH + "\" \"" +
                      shaderPath.string() + "\" --toy --headless --frames 1 "
                      "--dump-ppm \"" + outDir.string() + "\"";
    int rc = std::system(cmd.c_str());
    std::filesystem::current_path(oldCwd);
    ASSERT_EQ(rc, 0);

    const std::filesystem::path ppmPath = outDir / kPpmFrame0Name;
    ASSERT_TRUE(std::filesystem::exists(ppmPath));

    // Just as a sanity check, make sure it did only 1 frame
    const std::filesystem::path ppmPathNext = outDir / kPpmFrame1Name;
    ASSERT_FALSE(std::filesystem::exists(ppmPathNext));

    PPMImage img = readPPM(ppmPath);
    ASSERT_GT(img.width, 0u);
    ASSERT_GT(img.height, 0u);

    uint32_t xLeft = img.width / 4;
    uint32_t xRight = (img.width * 3) / 4;
    uint32_t yTop = img.height / 4;
    uint32_t yBottom = (img.height * 3) / 4;

    EXPECT_EQ(pixelAt(img, xLeft, yTop), (std::array<uint8_t, 3>{255, 0, 0}));
    EXPECT_EQ(pixelAt(img, xRight, yTop),
              (std::array<uint8_t, 3>{0, 255, 0}));
    EXPECT_EQ(pixelAt(img, xLeft, yBottom),
              (std::array<uint8_t, 3>{0, 0, 0}));
    EXPECT_EQ(pixelAt(img, xRight, yBottom),
              (std::array<uint8_t, 3>{0, 0, 255}));
}
