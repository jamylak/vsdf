# VSDF
Vulkan SDF Renderer + Hot Reloader

[![Linux Docker CI](https://github.com/jamylak/vsdf/actions/workflows/linux-docker-ci.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/linux-docker-ci.yml)
[![macOS CI](https://github.com/jamylak/vsdf/actions/workflows/macos-ci.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/macos-ci.yml)
[![Linux Smoke Tests](https://github.com/jamylak/vsdf/actions/workflows/linux-smoke-tests.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/linux-smoke-tests.yml)
[![Windows Build & Test](https://github.com/jamylak/vsdf/actions/workflows/windows-build-test.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/windows-build-test.yml)

![Preview](https://i.imgur.com/88KG4NL.gif)

Quickstart: see [QUICKSTART.md](QUICKSTART.md) for install + first shader.

Render an SDF like ShaderToy using Vulkan and hot reload based on frag shader changes.
That way you can use your favourite editor / LSP and also utilise git.

## Platforms
Supports macOS, Linux, and Windows with native file watcher implementations for each platform.

| OS | Support |
| --- | --- |
| Windows | ✅ Supported |
| Linux | ✅ Supported |
| macOS | ✅ Supported |

## Mac Dev Setup (Homebrew)
Install Vulkan + deps with Homebrew (Quickstart + macOS CI):
```sh
brew install molten-vk vulkan-loader glslang glfw glm spdlog vulkan-tools googletest
```

### Mac Lunar Setup (Optional)

https://vulkan.lunarg.com/sdk/home

Then follow the steps to do `sudo ./install_vulkan.py` in *SDK System Paths* section

#### Example `VULKAN_SDK` Env Var
`VULKAN_SDK $HOME/VulkanSDK/1.4.328.1/macOS`

## Linux Dev Setup (Ubuntu/Debian)
Install dependencies:
```sh
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build \
  libgtest-dev libspdlog-dev \
  libglfw3 libglfw3-dev libvulkan-dev \
  glslang-tools glslang-dev libglm-dev \
```

## Windows Dev Setup

1. Install vcpkg (if not already installed):
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```
2. Install dependencies using vcpkg (includes Vulkan):
   ```powershell
   vcpkg install vulkan:x64-windows glfw3:x64-windows glslang:x64-windows spdlog:x64-windows glm:x64-windows gtest:x64-windows
   vcpkg integrate install
   ```

Note: vcpkg provides Vulkan headers and loader, eliminating the need for a separate Vulkan SDK installation.

## Build

### Linux/macOS
```sh
cmake -B build .
cmake --build build
./build/vsdf {filepath}.frag
```

### Windows
```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" .
cmake --build build --config Release
.\build\Release\vsdf.exe {filepath}.frag
```

## Usage
### With shader toy shaders (most seem to work)
```sh
./build/vsdf --toy path/to/shader.frag
```

### Offline MP4 encoding (FFmpeg / H.264 via libx264):
```sh
./build/vsdf --toy shaders/testtoyshader.frag --frames 100 --ffmpeg-output out.mp4
```

### Example test command using a sample shader in this repo
```sh
./build/vsdf --toy shaders/testtoyshader.frag
```

Note: That if you use `--toy` we will prepend a template in
`shaders/toytemplate.frag` which sets up the push constants
like `iTime` and we will also follow this logic.

```cpp
if (useToyTemplate) {
    Client = glslang::EShClientOpenGL;
    ClientVersion = glslang::EShTargetOpenGL_450;
} else {
    Client = glslang::EShClientVulkan;
    ClientVersion = glslang::EShTargetVulkan_1_0;
}
```

### Manually create your own vulkan compatible shader
If you don't want any template prepended or you have issues
loading that way, I recommend copying `shaders/vulktemplate.frag`
and adjusting it to your liking

- See `shaders/vulktemplate.frag` to see how push constants
  are passed in
```sh
./build/vsdf path/to/shader.frag
```

### CLI Flags
- `--toy` Prepend the ShaderToy-compatible template
- `--frames <N>` Render N frames then exit (helps CI)
- `--headless` Hide the GLFW window (pair with `xvfb-run` in CI)
- `--log-level <trace|debug|info|warn|error|critical|off>` Set spdlog verbosity (default: info)
- `--debug-dump-ppm <dir>` Copy the swapchain image before present (adds a stall); mainly for smoke tests or debugging
- `--ffmpeg-output <file>` Enable offline encoding; output file path (requires `--frames`)
- `--ffmpeg-fps <N>` Output FPS (default: 30)
- `--ffmpeg-crf <N>` Quality for libx264 (default: 20; lower is higher quality)
- `--ffmpeg-preset <name>` libx264 preset (default: slow)
- `--ffmpeg-codec <name>` FFmpeg codec (default: libx264)
- `--ffmpeg-width <N>` Output width (default: 1280)
- `--ffmpeg-height <N>` Output height (default: 720)
- `--ffmpeg-ring-buffer-size <N>` Ring buffer size for offline render (default: 2)

### Test Dumping 1 frame
```sh
./build/vsdf shaders/debug_quadrants.frag --toy --headless --frames 1 --debug-dump-ppm out_ppm
```

Now in `out_ppm/` you should see an image with 4 quadrants:
- bottom-left: black
- bottom-right: blue
- top-left: red
- top-right: green

Also by running the test suite it will check this automatically

## Test Build

### Linux/macOS
```sh
cmake -B build -DBUILD_TESTS=ON -DDEBUG=ON
cmake --build build
./build/tests/vsdf_tests
./build/tests/filewatcher/filewatcher_tests
```

### Windows
```powershell
cmake -B build -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" .
cmake --build build --config Debug
.\build\tests\vsdf_tests\Debug\vsdf_tests.exe
.\build\tests\filewatcher\Debug\filewatcher_tests.exe
```

## Nix Develop Shell
```sh
nix develop
```

### Credits:
- https://shadertoy.com
- https://iquilezles.org/
- https://www.youtube.com/playlist?list=PL0JVLUVCkk-l7CWCn3-cdftR0oajugYvd (zeux)
- https://github.com/SaschaWillems/Vulkan

### CPP Guidelines
(I should try follow this but haven't gotten through it all yet)
https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
