# VSDF Quickstart

Go from zero to your first shader in minutes.

## 1) Install + Build

### macOS (Homebrew)
Install Vulkan + deps with Homebrew:
```sh
brew install molten-vk vulkan-loader glslang glfw glm spdlog vulkan-tools
```

Build:
```sh
cmake -B build .
cmake --build build
```

### Linux (Ubuntu/Debian)
Install dependencies, then build:
```sh
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build \
  libgtest-dev libspdlog-dev \
  libglfw3 libglfw3-dev libvulkan-dev \
  glslang-tools glslang-dev libglm-dev \
```

Build:
```sh
cmake -B build .
cmake --build build
```

### Windows (vcpkg)
1. Install vcpkg:
   ```powershell
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```
2. Install deps (includes Vulkan):
   ```powershell
   vcpkg install vulkan:x64-windows glfw3:x64-windows glslang:x64-windows spdlog:x64-windows glm:x64-windows gtest:x64-windows
   vcpkg integrate install
   ```
3. Build:
   ```powershell
   cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" .
   cmake --build build --config Release
   ```

## 2) Launch the sample shader
```sh
./build/vsdf --toy shaders/testtoyshader.frag
```

## 3) Make your own shader (copy and run)
Saving your file hot reloads.

### ShaderToy-style (template prepended)
```sh
cp shaders/toytemplate.frag shaders/myshader.frag
./build/vsdf --toy shaders/myshader.frag
```

### Vulkan-style (no template)
```sh
cp shaders/vulktemplate.frag shaders/myshader.frag
./build/vsdf shaders/myshader.frag
```

## Notes
- `--toy` prepends `shaders/toytemplate.frag` to set up push constants in the format
ShaderToy uses, e.g., `iTime` as well as `main()` etc.

## Example: save a ShaderToy and run it locally
1. Open the ShaderToy and copy the fragment shader code:
   https://www.shadertoy.com/view/Xds3zN
2. Save it as `shaders/raymarching_primitives.frag`.
3. Run it with the template prepended:
```sh
./build/vsdf --toy shaders/raymarching_primitives.frag
```
