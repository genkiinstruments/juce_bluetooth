{
  description = "Development environment with JUCE dependencies";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
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
            ladspa-sdk

            bluez
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
              pkgs.ladspa-sdk
            ]}:$LD_LIBRARY_PATH
          '';
        };
      });
}
