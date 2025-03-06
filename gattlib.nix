{ lib
, stdenv
, fetchFromGitHub
, cmake
, pkg-config
, bluez
, dbus
, glib
, readline
, pcre
}:

stdenv.mkDerivation rec {
  pname = "gattlib";
  version = "0.2";

  src = fetchFromGitHub {
    owner = "labapart";
    repo = "gattlib";
    rev = "1580056bce260f0d8ad7ccc8b105c34057cd1fbd";
    sha256 = "sha256-ypAzH14mCcytbv9oiXj63GiN4KBbbAI1pdoXrA2LxRc=";
  };

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [
    bluez
    dbus
    glib
    readline
    pcre
  ];

  cmakeFlags = [
    "-DGATTLIB_FORCE_DBUS=ON"
  ];

  # Add pkg-config paths for the build system to find the libraries
  CFLAGS = [
    "-I${bluez}/include"
    "-I${readline}/include"
    "-I${pcre}/include"
  ];

  PKG_CONFIG_PATH = lib.makeSearchPath "lib/pkgconfig" [
    bluez
    readline
    pcre
  ];

  meta = with lib; {
    description = "A library to access GATT information on BLE devices";
    homepage = "https://github.com/labapart/gattlib";
    license = licenses.mit;
    platforms = platforms.linux;
    broken = stdenv.isDarwin;
  };
}
