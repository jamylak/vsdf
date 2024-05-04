#ifndef TEST_UTILS_H
#define TEST_UTILS_H
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
#endif // TEST_UTILS_H
