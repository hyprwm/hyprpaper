{
  lib,
  stdenv,
  pkg-config,
  cmake,
  cairo,
  expat,
  file,
  fribidi,
  hyprlang,
  libdatrie,
  libGL,
  libjpeg,
  libselinux,
  libsepol,
  libthai,
  libwebp,
  pango,
  pcre,
  pcre2,
  util-linux,
  wayland,
  wayland-protocols,
  wayland-scanner,
  libXdmcp,
  debug ? false,
  version ? "git",
}:
stdenv.mkDerivation {
  pname = "hyprpaper" + lib.optionalString debug "-debug";
  inherit version;

  src = ../.;

  cmakeBuildType =
    if debug
    then "Debug"
    else "Release";

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  buildInputs = [
    cairo
    expat
    file
    fribidi
    hyprlang
    libdatrie
    libGL
    libjpeg
    libselinux
    libsepol
    libthai
    libwebp
    pango
    pcre
    pcre2
    wayland
    wayland-protocols
    wayland-scanner
    libXdmcp
    util-linux
  ];

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprpaper";
    description = "A blazing fast wayland wallpaper utility with IPC controls";
    license = licenses.bsd3;
    platforms = platforms.linux;
    mainProgram = "hyprpaper";
  };
}
