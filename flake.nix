{
  description = "Development environment with JUCE dependencies and gattlib";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        gattlib = pkgs.callPackage ./gattlib.nix {};
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            # Your existing packages
            pkg-config
            alsa-lib
            freetype
            fontconfig
            libGL
            curl
            webkitgtk
            gtk3
            xorg.libX11
            xorg.libXrandr
            xorg.libXcursor
            xorg.libXinerama

            bluez
            gattlib
          ];

          shellHook = ''
            export LD_LIBRARY_PATH=${pkgs.lib.makeLibraryPath [
              pkgs.alsa-lib
              pkgs.freetype
              pkgs.fontconfig
              pkgs.libGL
              pkgs.curl
              pkgs.webkitgtk
              pkgs.gtk3
              pkgs.xorg.libX11
              pkgs.xorg.libXrandr
              pkgs.xorg.libXcursor
              pkgs.xorg.libXinerama
              pkgs.bluez
              gattlib
            ]}:$LD_LIBRARY_PATH

            # Export the gattlib location explicitly
            export GATTLIB_DIR=${gattlib}
            export GATTLIB_INCLUDE_DIR=${gattlib}/include
            export GATTLIB_LIBRARY_DIR=${gattlib}/lib

            # Add to PKG_CONFIG_PATH
            export PKG_CONFIG_PATH=${gattlib}/lib/pkgconfig:$PKG_CONFIG_PATH
          '';
        };

        # Optionally, make gattlib available as a package
        packages.gattlib = gattlib;
        packages.default = gattlib;
      });
}
