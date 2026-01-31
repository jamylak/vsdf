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
, fetchFromGitHub
}:

let
  _ = assert lib.versionAtLeast glfw.version "3.4"; null;
  volk = fetchFromGitHub {
    owner = "zeux";
    repo = "volk";
    rev = "d34b5e0d46b28c22d69b97ee7da074b6e68d9e25";
    sha256 = "0fm6i4bb1c4dj148x8zvrqzc18cdvnq71zl86l38nnvmy5ncvz69";
  };
in
stdenv.mkDerivation {
  pname = "vsdf";
  version = "0.1.0";

  src = ./.;

  postUnpack = ''
    rmdir $sourceRoot/external/volk || true
    cp -r ${volk} $sourceRoot/external/volk
    chmod -R +w $sourceRoot/external/volk
  '';

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
