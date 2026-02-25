{
  lib,
  stdenv,
  pkg-config,
  cmake,
  aquamarine,
  cairo,
  expat,
  file,
  fribidi,
  hyprgraphics,
  hyprlang,
  hyprutils,
  hyprtoolkit,
  hyprwire,
  hyprwayland-scanner,
  libGL,
  libdatrie,
  libdrm,
  libjpeg,
  libjxl,
  libselinux,
  libsepol,
  libthai,
  libXdmcp,
  libwebp,
  pango,
  pcre,
  pcre2,
  util-linux,
  wayland,
  wayland-protocols,
  wayland-scanner,
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

  depsBuildBuild = [
    pkg-config
  ];

  cmakeBuildType = if debug then "Debug" else "Release";

  nativeBuildInputs = [
    cmake
    hyprwayland-scanner
    hyprwire
    pkg-config
    wayland-scanner
  ];

  buildInputs = [
    aquamarine
    cairo
    expat
    file
    fribidi
    hyprgraphics
    hyprlang
    hyprutils
    hyprtoolkit
    hyprwire
    libGL
    libdatrie
    libdrm
    libjpeg
    libjxl
    libselinux
    libsepol
    libthai
    libwebp
    libXdmcp
    pango
    pcre
    pcre2
    wayland
    wayland-protocols
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
