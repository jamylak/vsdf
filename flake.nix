# Based on
# https://github.com/nixvital/nix-based-cpp-starterkit/tree/main
{
  description = "vsdf";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/25.11";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, ... }@inputs: inputs.utils.lib.eachSystem [
    "x86_64-linux"
    "i686-linux"
    "aarch64-linux"
    "x86_64-darwin"
    "aarch64-darwin"
  ]
    (system:
      let
        pkgs = import nixpkgs {
          inherit system;

          # Add overlays here if you need to override the nixpkgs
          # official packages.
          overlays = [ ];

          # Uncomment this if you need unfree software (e.g. cuda) for
          # your project.
          #
          # config.allowUnfree = true;
        };
        glfwX11 = pkgs.glfw.override {
          waylandSupport = false;
          x11Support = true;
        };
      in
      {
        devShells.default = pkgs.mkShell rec {
          name = "vsdf";

          packages = with pkgs; [
            # Development Tools
            llvmPackages_21.clang
            cmake
            cmakeCurses
            ninja

            # Development time dependencies
            gtest

            # Dependencies
            glfwX11
            glm
            spdlog
            vulkan-loader
            vulkan-headers
            ffmpeg
            pkg-config

            # shaderc
            spirv-tools
            glslang
          ];

          # Setting up the environment variables you need during
          # development.
          shellHook =
            let
              icon = "f121";
            in
            ''
              export PS1="$(echo -e '\u${icon}') {\[$(tput sgr0)\]\[\033[38;5;228m\]\w\[$(tput sgr0)\]\[\033[38;5;15m\]} (${name}) \\$ \[$(tput sgr0)\]"
              if [ "$(uname -s)" = "Linux" ]; then
                export GLFW_PLATFORM=x11
              fi
            '';
        };

        packages.default = pkgs.callPackage ./default.nix { glfw = glfwX11; };
      });
}
