{
  description = "JUCE Bluetooth cross-compilation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        # Use a simpler cross-compiler setup
        crossPkgsSimple = pkgs.pkgsCross.aarch64-multiplatform.buildPackages;

      in {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
          ];

          buildInputs = with pkgs; [
            # Cross compiler toolchain
            crossPkgsSimple.gcc

            # Native dependencies for build
            juce
            fmt
            microsoft-gsl
            range-v3
          ];

          shellHook = ''
            export CC=${crossPkgsSimple.gcc}/bin/aarch64-unknown-linux-gnu-gcc
            export CXX=${crossPkgsSimple.gcc}/bin/aarch64-unknown-linux-gnu-g++

            # Set include paths for native dependencies
            export CXXFLAGS="-I${pkgs.fmt}/include -I${pkgs.microsoft-gsl}/include -I${pkgs.range-v3}/include -I${pkgs.juce}/include"

            echo "Cross-compilation environment ready!"
            echo "CC=$CC"
            echo "CXX=$CXX"

            if [ "$SHELL" != "$(which fish)" ]; then
              exec fish
            fi
          '';
        };
      }
    );
}
