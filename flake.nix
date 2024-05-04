# Based on
# https://github.com/nixvital/nix-based-cpp-starterkit/tree/main
{
  description = "vsdf";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/23.11";
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
      in
      {
        devShells.default = pkgs.mkShell rec {
          name = "vsdf";

          packages = with pkgs; [
            # Development Tools
            llvmPackages_14.clang
            cmake
            cmakeCurses

            # Development time dependencies
            gtest

            # Dependencies
            glfw
            glm
            spdlog
            boost
            vulkan-loader
            vulkan-headers
          ];

          # Setting up the environment variables you need during
          # development.
          shellHook =
            let
              icon = "f121";
            in
            ''
              export PS1="$(echo -e '\u${icon}') {\[$(tput sgr0)\]\[\033[38;5;228m\]\w\[$(tput sgr0)\]\[\033[38;5;15m\]} (${name}) \\$ \[$(tput sgr0)\]"
            '';
        };

        packages.default = pkgs.callPackage ./default.nix { };
      });
}
