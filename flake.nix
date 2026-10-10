{
  description = "Zip BOF and BOF-PE development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    # Renders docs/demo; pinned so re-rendering only changes when bumped.
    beaconcast.url = "github:rubyattack3r/beaconcast/v0.1.0";
  };

  outputs = inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];

      perSystem = { pkgs, inputs', ... }: {
        # Zig compiles every target; LLVM provides the object inspection tools,
        # and clang the sanitizer runtimes Zig lacks.
        devShells.default = pkgs.mkShellNoCC {
          packages = [
            pkgs.actionlint
            pkgs.clang # sanitizer builds only
            pkgs.cppcheck
            pkgs.gnumake
            pkgs.libarchive
            pkgs.llvm
            pkgs.perl
            pkgs.shellcheck
            pkgs.unzip
            pkgs.zig
            pkgs.zip
          ];
        };

        # Renders docs/demo with `nix develop .#demo -c make demo`.
        devShells.demo = pkgs.mkShellNoCC {
          packages = [
            inputs'.beaconcast.packages.default
            pkgs.gnumake
          ];
        };
      };
    };
}
