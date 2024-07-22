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
  hyprutils,
  hyprwayland-scanner,
  libdatrie,
  libGL,
  libjpeg,
  libselinux,
  libsepol,
  libthai,
  libwebp,
  mesa,
  pango,
  pcre,
  pcre2,
  util-linux,
  wayland,
  wayland-protocols,
  wayland-scanner,
  xorg,
  commit,
  debug ? false,
  version ? "git",
}:
stdenv.mkDerivation {
  pname = "hyprpaper" + lib.optionalString debug "-debug";
  inherit version;

  src = ../.;

  prePatch = ''
    substituteInPlace src/main.cpp \
      --replace GIT_COMMIT_HASH '"${commit}"'
  '';

  cmakeBuildType =
    if debug
    then "Debug"
    else "Release";

  nativeBuildInputs = [
    cmake
    hyprwayland-scanner
    pkg-config
  ];

  buildInputs = [
    cairo
    expat
    file
    fribidi
    hyprlang
    hyprutils
    libdatrie
    libGL
    libjpeg
    libselinux
    libsepol
    libthai
    libwebp
    mesa
    pango
    pcre
    pcre2
    wayland
    wayland-protocols
    wayland-scanner
    xorg.libXdmcp
    util-linux
  ];

  meta = with lib; {
    description = "A blazing fast wayland wallpaper utility with IPC controls";
    homepage = "https://github.com/hyprwm/hyprpaper";
    license = licenses.bsd3;
    mainProgram = "hyprpaper";
    platforms = platforms.linux;
  };
}
