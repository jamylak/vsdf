# VSDF
Vulkan SDF Renderer + Hot Reloader

[![Continuous Integration](https://github.com/jamylak/vsdf/actions/workflows/test.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/test.yml)
[![Smoke Test](https://github.com/jamylak/vsdf/actions/workflows/smoketest.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/smoketest.yml)
[![Windows CI](https://github.com/jamylak/vsdf/actions/workflows/windows-test.yml/badge.svg)](https://github.com/jamylak/vsdf/actions/workflows/windows-test.yml)

![Preview](https://i.imgur.com/88KG4NL.gif)

Render an SDF like ShaderToy using Vulkan and hot reload based on frag shader changes.
That way you can use your favourite editor / LSP and also utilise git.

## Platforms
Supports macOS, Linux, and Windows with native file watcher implementations for each platform.

## Mac Dev Setup (with Lunar)

https://vulkan.lunarg.com/sdk/home

Then follow the steps to do `sudo ./install_vulkan.py` in *SDK System Paths* section

### Example `VULKAN_SDK` Env Var
`VULKAN_SDK $HOME/VulkanSDK/1.4.328.1/macOS`

## Windows Dev Setup

1. Download and install the Vulkan SDK from https://vulkan.lunarg.com/sdk/home
2. Install vcpkg (if not already installed):
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```
3. Install dependencies using vcpkg:
   ```powershell
   vcpkg install glfw3:x64-windows glslang:x64-windows spdlog:x64-windows glm:x64-windows gtest:x64-windows
   vcpkg integrate install
   ```

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

Headless / CI-friendly single-frame run (hide the window and exit after N frames):
```sh
./build/vsdf --toy shaders/testtoyshader.frag --frames 1 --headless
```

## Usage
### With shader toy shaders (most seem to work)
```sh
./build/vsdf --toy path/to/shader.frag
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
