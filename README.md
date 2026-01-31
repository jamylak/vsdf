# VSDF
Vulkan SDF Renderer + Video Recorder + Hot Reloader

[![Linux Docker CI](https://github.com/jamylak/vsdf/actions/workflows/linux-docker-ci.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/linux-docker-ci.yml)
[![macOS CI](https://github.com/jamylak/vsdf/actions/workflows/macos-ci.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/macos-ci.yml)
[![Linux Smoke Tests](https://github.com/jamylak/vsdf/actions/workflows/linux-smoke-tests.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/linux-smoke-tests.yml)
[![Windows Build & Test](https://github.com/jamylak/vsdf/actions/workflows/windows-build-test.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/windows-build-test.yml)

![Preview](https://i.imgur.com/88KG4NL.gif)

## License
**VSDF is licensed under GPL-3.0**

**Quickstart:** See [QUICKSTART.md](QUICKSTART.md) for installation and your first shader in minutes.

**Shell Integration:** (Experimental) See [SHELL_INTEGRATION.md](SHELL_INTEGRATION.md) for instant shader development with one command.

Render an SDF like ShaderToy using Vulkan and hot reload based on frag shader changes.
That way you can use your favourite editor / LSP and also utilise git.

## Platforms
Supports macOS, Linux, and Windows with native file watcher implementations for each platform.

| OS | Support |
| --- | --- |
| Windows | ✅ Supported |
| Linux | ✅ Supported |
| macOS | ✅ Supported |

## Mac Setup (Homebrew)

**Easiest way to install vsdf:**

This will install MoltenVK
```sh
brew install jamylak/vsdf/vsdf
```

**For building from source or manual dependency management:**
Install Vulkan + deps with Homebrew:
```sh
brew install molten-vk vulkan-loader glslang glfw glm spdlog vulkan-tools googletest ffmpeg
# Note: FFmpeg is optional; set `-DDISABLE_FFMPEG=ON` (see `CMakeLists.txt`)
```

### Mac Lunar Setup (Optional)

https://vulkan.lunarg.com/sdk/home

Then follow the steps to do `sudo ./install_vulkan.py` in *SDK System Paths* section

#### Example `VULKAN_SDK` Env Var
`VULKAN_SDK $HOME/VulkanSDK/1.4.328.1/macOS`

## Linux Binary Installation

Pre-built binaries for Linux are available in the [GitHub Releases](https://github.com/jamylak/vsdf/releases) page.
Download the latest `vsdf-linux` binary, make it executable, and move it to a directory in your `PATH`.
The only dependency is Vulkan.

```sh
LATEST_RELEASE_TAG=$(curl -sL https://api.github.com/repos/jamylak/vsdf/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/')
DOWNLOAD_URL="https://github.com/jamylak/vsdf/releases/download/${LATEST_RELEASE_TAG}/vsdf-linux-x86_64.tar.gz"
echo "Downloading from: ${DOWNLOAD_URL}"
curl -LO "${DOWNLOAD_URL}"
tar -xzf vsdf-linux-x86_64.tar.gz
chmod +x linux/vsdf
sudo mv linux/vsdf /usr/local/bin/vsdf
rm -rf vsdf-linux-x86_64.tar.gz linux # Clean up downloaded files
```

## Windows Binary Installation (no `ffmpeg`)

Pre-built Windows binaries (built without `ffmpeg`) are available on the [GitHub Releases](https://github.com/jamylak/vsdf/releases) page.
Download the latest release zip and run `vsdf.exe` from the extracted folder.

```powershell
$tag = (Invoke-RestMethod https://api.github.com/repos/jamylak/vsdf/releases/latest).tag_name
$zip = "vsdf-windows-x86_64-disable_ffmpeg.zip"
$url = "https://github.com/jamylak/vsdf/releases/download/$tag/$zip"
Invoke-WebRequest -Uri $url -OutFile $zip
Expand-Archive $zip -DestinationPath vsdf
.\vsdf\vsdf.exe --version
```

## Linux Dev Setup (Ubuntu/Debian)
Install dependencies:
```sh
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build \
  libgtest-dev libspdlog-dev \
  libglfw3 libglfw3-dev libvulkan-dev \
  glslang-tools glslang-dev libglm-dev \
  # (Optional) set -DDISABLE_FFMPEG=ON to skip \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
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
   # Note: ffmpeg is optional; set `-DDISABLE_FFMPEG=ON` (see `CMakeLists.txt`) to build without it
   vcpkg install ffmpeg[avcodec,avformat,swscale]:x64-windows
   vcpkg integrate install
   ```

## Build

### Linux/macOS
```sh
# ffmpeg is optional; set `-DDISABLE_FFMPEG=ON` (see `CMakeLists.txt`) to build without it.
git submodule update --init --recursive
cmake -B build .
cmake --build build
./build/vsdf {filepath}.frag
```

### Windows
```powershell
# ffmpeg is optional; set `-DDISABLE_FFMPEG=ON` (see `CMakeLists.txt`) to build without it.
git submodule update --init --recursive
cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" .
cmake --build build --config Release
.\build\Release\vsdf.exe {filepath}.frag
```

## Install

### System-wide Install

To make `vsdf` available as a command in your shell, you can install it to a standard system directory like `/usr/local`.

**Linux/macOS**
```sh
# Pick somewhere in your PATH and install it there
cmake --install build --prefix /usr/local
vsdf {filepath}.frag
```

On Windows, you can also install to a custom location and add that location to your `PATH` environment variable to make it accessible from any command prompt.

## Usage
### With shader toy shaders (most seem to work)
```sh
vsdf --new-toy example.frag
vsdf --toy example.frag
```

### Offline MP4 encoding (`ffmpeg` / H.264 via libx264):
```sh
vsdf --toy example.frag --frames 100 --ffmpeg-output out.mp4
```

### Example test command using a sample shader in this repo
```sh
vsdf --toy example.frag
```

Note: That if you use `--toy` we will prepend a template
which links up the push constants
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

### CLI Flags
- `--help` Show this help message
- `--version` Show version information
- `--new-toy [name]` Create a new shader file with starter template. Prints the filename and exits. Generates random name like my_new_toy_12345.frag if not provided.
- `--template <name>` Template to use with `--new-toy` (default, plot)
- `--toy` Use ShaderToy-style template wrapper
- `--no-focus` Don't steal window focus on startup and float
- `--headless` Hide the GLFW window (pair with `xvfb-run` in CI)
- `--frames <N>` Render N frames then exit
- `--log-level <trace|debug|info|warn|error|critical|off>` Set `spdlog` verbosity (default: info)
- `--debug-dump-ppm <dir>` Copy the swapchain image before present (adds a stall); mainly for smoke tests or debugging
- `--ffmpeg-output <file>` Enable offline encoding; output file path (requires `--frames`)
- `--ffmpeg-fps <N>` Output FPS (default: 30)
- `--ffmpeg-crf <N>` Quality for libx264 (default: 20; lower is higher quality)
- `--ffmpeg-preset <name>` libx264 preset (default: slow)
- `--ffmpeg-codec <name>` FFmpeg codec (default: libx264)
- `--ffmpeg-width <N>` Output width (default: 1280)
- `--ffmpeg-height <N>` Output height (default: 720)
- `--ffmpeg-ring-buffer-size <N>` Ring buffer size for offline render (default: 2)

## Test Build

### Linux/macOS
```sh
git submodule update --init --recursive
cmake -B build -DBUILD_TESTS=ON -DDEBUG=ON
cmake --build build
./build/tests/vsdf_tests
./build/tests/filewatcher/filewatcher_tests
```

### Windows
```powershell
git submodule update --init --recursive
cmake -B build -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" .
cmake --build build --config Debug
.\build\tests\vsdf_tests\Debug\vsdf_tests.exe
.\build\tests\filewatcher\Debug\filewatcher_tests.exe
```

## Nix
### Nix Develop Shell
```sh
nix develop
```

### Nix Run / Install (one command)
Run without installing (builds and runs):
```sh
nix run github:jamylak/vsdf
```

Install into your profile (then `vsdf` is on PATH):
```sh
nix profile install github:jamylak/vsdf
```

Open a one-off shell with `vsdf` available:
```sh
nix shell github:jamylak/vsdf
```

### Home Manager (flake) install
```nix
# flake.nix
{
  inputs.vsdf.url = "github:jamylak/vsdf";
}
```
```nix
# home.nix
{ inputs, pkgs, ... }:
{
  home.packages = [
    inputs.vsdf.packages.${pkgs.system}.default
  ];
}
```

### Manually create your own vulkan compatible shader
If you don't want any template prepended or you want
to use a Vulkan shader directly you can copy `shaders/vulktemplate.frag`
and adjust it to your liking

- See `shaders/vulktemplate.frag` to see how push constants
  are passed in

```sh
# Then call it without the --toy flag
vsdf path/to/shader.frag
```

The binary uses [volk](https://github.com/zeux/volk) to dynamically find Vulkan at runtime, so it works regardless of installation location.


### Credits:
- https://shadertoy.com
- https://iquilezles.org/
- https://www.youtube.com/playlist?list=PL0JVLUVCkk-l7CWCn3-cdftR0oajugYvd (zeux)
- https://github.com/SaschaWillems/Vulkan

### CPP Guidelines
(I should try follow this but haven't gotten through it all yet)
https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
