# VSDF
Vulkan SDF Renderer + Hot Reloader

![Preview](https://i.imgur.com/88KG4NL.gif)

Render an SDF like ShaderToy using Vulkan and hot reload based on frag shader changes.
That way you can use your favourite editor / LSP and also utilise git.

## Platforms
Supports Mac & Linux currently because it contains filewatcher implementations for those platforms so far but could add Windows and then the rest of the code should work on there...

## Mac Dev Setup (with Lunar)

https://vulkan.lunarg.com/sdk/home

Then follow the steps to do `sudo ./install_vulkan.py` in *SDK System Paths* section

### Example `VULKAN_SDK` Env Var
`VULKAN_SDK $HOME/VulkanSDK/1.4.328.1/macOS`

## Build
```sh
cmake -B build .
cmake --build build
./build/vsdf {filepath}.frag
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

## Test Build
```sh
cmake -B build -DBUILD_TESTS=ON -DDEBUG=ON
cmake --build build
./build/tests/vsdf_tests
./build/tests/filewatcher/filewatcher_tests
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
