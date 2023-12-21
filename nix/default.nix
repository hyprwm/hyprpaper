{
  lib,
  stdenv,
  pkg-config,
  cmake,
  ninja,
  cairo,
  file,
  fribidi,
  libdatrie,
  libGL,
  libjpeg,
  libselinux,
  libsepol,
  libthai,
  libwebp,
  pango,
  pcre,
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

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
  ];

  buildInputs = [
    cairo
    file
    fribidi
    libdatrie
    libGL
    libjpeg
    libselinux
    libsepol
    libthai
    libwebp
    pango
    pcre
    wayland
    wayland-protocols
    wayland-scanner
    libXdmcp
    util-linux
  ];

  configurePhase = ''
    runHook preConfigure

    make protocols

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild

    make release

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/{bin,share/licenses}

    install -Dm755 build/hyprpaper -t $out/bin
    install -Dm644 LICENSE -t $out/share/licenses/hyprpaper

    runHook postInstall
  '';

  meta = with lib; {
    homepage = "https://github.com/hyprwm/hyprpaper";
    description = "A blazing fast wayland wallpaper utility with IPC controls";
    license = licenses.bsd3;
    platforms = platforms.linux;
    mainProgram = "hyprpaper";
  };
}
