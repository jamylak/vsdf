#ifndef TEST_UTILS_H
#define TEST_UTILS_H
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

class TempShaderFile {
  public:
    TempShaderFile(const std::string &filename, const std::string &content)
        : filename_(filename) {
        std::ofstream shaderFile(filename_);
        shaderFile << content;
        shaderFile.close();
    }

    ~TempShaderFile() { std::remove(filename_.c_str()); }

    const std::string &filename() const { return filename_; }

  private:
    std::string filename_;
};

inline bool shouldSkipSmokeTests() {
    const char *ciEnv = std::getenv("CI");
    const char *smokeEnv = std::getenv("VSDF_SMOKE_TESTS");
    const bool inCi = ciEnv && std::string(ciEnv) == "true";
    const bool smokeEnabled = smokeEnv && std::string(smokeEnv) == "1";
    return inCi && !smokeEnabled;
}

// Non-throwing log helper: if the log is missing/unreadable, return empty
// so the test failure still reports the original command error.
[[nodiscard]] inline std::string readLogFileToString(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }
    const std::ifstream::pos_type size = file.tellg();
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(size), '\0');
    file.seekg(0, std::ios::beg);
    file.read(out.data(), out.size());
    return out;
}
#endif // TEST_UTILS_H
