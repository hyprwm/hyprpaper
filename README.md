# hyprpaper

Hyprpaper is a simple and fast wallpaper utility for Hyprland with the ability to dynamically change wallpapers through sockets.

# Features
 - Per-output wallpapers
 - fill, tile, cover or contain modes
 - fractional scaling support
 - IPC for fast wallpaper switches

# Installation

[Arch Linux](https://archlinux.org/packages/extra/x86_64/hyprpaper/): `pacman -S hyprpaper`

[OpenSuse Linux](https://software.opensuse.org/package/hyprpaper): `zypper install hyprpaper`

## Manual:

### Dependencies
The development files of these packages need to be installed on the system for `hyprpaper` to build correctly.
(Development packages are usually suffixed with `-dev` or `-devel` in most distros' repos).
- hyprtoolkit
- hyprlang
- hyprutils
- hyprwire

### Building

Building is done via CMake:

```sh
cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:PATH=/usr -S . -B ./build
cmake --build ./build --config Release --target hyprpaper -j`nproc 2>/dev/null || getconf _NPROCESSORS_CONF`
```

Install with:

```sh
cmake --install ./build
```