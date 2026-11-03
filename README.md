# hyprpaper

Hyprpaper is a simple and fast wallpaper utility for Hyprland with the ability to dynamically change wallpapers through sockets.

# Features
 - Per-output wallpapers
 - fill, tile, cover, contain or span modes
 - fractional scaling support
 - IPC for fast wallpaper switches

# Fit modes
`fit_mode` controls how each wallpaper is placed on its output: `cover` (default), `contain`, `fill` (stretch), `tile`.

`span` splits a single image across every output it is assigned to, so the picture appears continuous across the physical monitor arrangement. Outputs sharing the same `wallpaper {}` entry form one span group; the image is mapped onto the group's bounding box (in Hyprland's logical layout coordinates) and each output shows the sub-region its layout rectangle covers. Placement within that canvas is chosen with an optional sub-mode: `span` / `span:cover` (default), `span:contain`, `span:stretch`.

Span requires Hyprland's IPC for the monitor layout; when it is unavailable (or an output's geometry can't be resolved) each output falls back to `cover`. A group of one output is equivalent to the matching plain mode.

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