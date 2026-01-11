#ifndef TEST_UTILS_H
#define TEST_UTILS_H
#include <cstdlib>
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
#endif // TEST_UTILS_H
