{ pkgs, ... }:
pkgs.mkShell {
  packages = with pkgs; [
    cmake
    ninja
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

  shellHook = with pkgs; ''
    export LD_LIBRARY_PATH=${
      lib.makeLibraryPath [
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
        ladspa-sdk
      ]
    }:$LD_LIBRARY_PATH
  '';
  env = { };
}
