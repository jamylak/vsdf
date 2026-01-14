{ lib
, stdenv
, cmake
, ninja
, glfw
, glm
, spdlog
, vulkan-loader
, vulkan-headers
, glslang
, spirv-tools
, llvmPackages_21
, ffmpeg
, pkg-config
}:

stdenv.mkDerivation {
  pname = "vsdf";
  version = "0.1.0";

  src = ./.;

  nativeBuildInputs = [
    cmake
    ninja
    llvmPackages_21.clang
    pkg-config
  ];

  buildInputs = [
    glfw
    glm
    spdlog
    vulkan-loader
    vulkan-headers
    glslang
    spirv-tools
    ffmpeg
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DBUILD_TESTS=OFF"
  ];

  meta = with lib; {
    description = "Vulkan SDF Renderer + Hot Reloader";
    homepage = "https://github.com/jamylak/vsdf";
    license = licenses.mit;
    platforms = platforms.linux ++ platforms.darwin;
  };
}
