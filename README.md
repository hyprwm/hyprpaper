# hyprpaper

Hyprpaper is a blazing fast wallpaper utility for Hyprland with the ability to dynamically change wallpapers through sockets. It will work on all wlroots-based compositors, though.

# Features
 - Per-output wallpapers
 - fill or contain modes
 - fractional scaling support
 - IPC for blazing fast wallpaper switches
 - preloading targets into memory

# Installation

[Arch Linux](https://archlinux.org/packages/extra/x86_64/hyprpaper/): `pacman -S hyprpaper`

[OpenSuse Linux](https://software.opensuse.org/package/hyprpaper): `zypper install hyprpaper`

## Manual:

### Dependencies
The development files of these packages need to be installed on the system for `hyprpaper` to build correctly.
(Development packages are usually suffixed with `-dev` or `-devel` in most distros' repos).
- wayland
- wayland-protocols
- pango
- cairo
- file
- libglvnd
- libglvnd-core
- libjpeg-turbo
- libwebp
- hyprlang

Please note hyprpaper > 0.5.0 depends on [hyprlang](https://github.com/hyprwm/hyprlang) which is new
and might not be packaged for your distro yet. If that's the case, build and install it from source.

To install all of these in Fedora, run this command:
```
sudo dnf install wayland-devel wayland-protocols-devel hyprlang-devel pango-devel cairo-devel file-devel libglvnd-devel libglvnd-core-devel libjpeg-turbo-devel libwebp-devel gcc-c++
```

On Arch:
```
sudo pacman -S ninja gcc wayland-protocols libjpeg-turbo libwebp pango cairo pkgconf cmake libglvnd wayland
```

On OpenSUSE:
```
sudo zypper install ninja gcc-c++ wayland-protocols-devel Mesa-libGLESv3-devel file-devel
```

### Building

Building is done via CMake:

```sh
cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:PATH=/usr -S . -B ./build
cmake --build ./build --config Release --target hyprpaper -j`nproc 2>/dev/null || getconf NPROCESSORS_CONF`
```

Install with:

```sh
cmake --install ./build
```

# Usage

Hyprpaper is controlled by the config, like this:

*~/.config/hypr/hyprpaper.conf*
```
preload = /path/to/image.png
#if more than one preload is desired then continue to preload other backgrounds
preload = /path/to/next_image.png
# .. more preloads

#set the default wallpaper(s) seen on initial workspace(s) --depending on the number of monitors used
wallpaper = monitor1,/path/to/image.png
#if more than one monitor in use, can load a 2nd image
wallpaper = monitor2,/path/to/next_image.png
# .. more monitors

#enable splash text rendering over the wallpaper
splash = true

#fully disable ipc
# ipc = off


```

Preload will tell Hyprland to load a particular image (supported formats: png, jpg, jpeg, webp). Wallpaper will apply the wallpaper to the selected output (`monitor` is the monitor's name, easily can be retrieved with `hyprctl monitors`. You can leave it empty to set all monitors without an active wallpaper. You can also use `desc:` followed by the monitor's description without the (PORT) at the end)

You may add `contain:` before the file path in `wallpaper=` to set the mode to contain instead of cover:

```
wallpaper = monitor,contain:/path/to/image.jpg
```

A Wallpaper ***cannot*** be applied without preloading. The config is ***not*** reloaded dynamically.

## Important note to the inner workings
Preload does exactly what it says. It loads the entire wallpaper into memory. This can result in around 8 - 20MB of mem usage. It is not recommended to preload every wallpaper you have, as it will be a) taking a couple seconds at the beginning to load and b) take 100s of MBs of disk and RAM usage.

Preload is meant only for situations in which you want a wallpaper to switch INSTANTLY when you issue a wallpaper keyword (e.g. wallpaper per workspace)

In any and all cases when you don't mind waiting 300ms for the wallpaper to change, consider making a script that:
 - preloads the new wallpaper
 - sets the new wallpaper
 - unloads the old wallpaper (to free memory)

# IPC
You can use `hyprctl hyprpaper` (if on Hyprland) to issue a keyword, for example

Example:

If your wallpapers are stored in *~/Pictures*, then make sure you have already preloaded the desired wallpapers in hyprpaper.conf.

*~/.config/hypr/hyprpaper.conf*
```
preload = ~/Pictures/myepicpng.png
preload = ~/Pictures/myepicpngToo.png
preload = ~/Pictures/myepicpngAlso.png
#... continue as desired, but be mindful of the impact on memory.
```

In the actual configuration for Hyprland, *hyprland.conf*, variables can be set for ease of reading and to be used as shortcuts in the bind command. The following example uses $w shorthand wallpaper variables:

*~/.config/hypr/hyprland.conf*
```
$w1 = hyprctl hyprpaper wallpaper "DP-1,~/Pictures/myepicpng.png"
$w2 = hyprctl hyprpaper wallpaper "DP-1,~/Pictures/myepicpngToo.png"
$w3 = hyprctl hyprpaper wallpaper "DP-1,~/Pictures/myepicpngAlso.png"
#yes use quotes around desired monitor and wallpaper
#... continued with desired amount
```
With the variables created we can now "exec" the actions.

Remember in Hyprland we can bind more than one action to a key so in the case where we'd like to change the wallpaper when we switch workspace we have to ensure that the actions are bound to the same key such as...

*~/.config/hypr/hyprland.conf*
```
bind=SUPER,1,workspace,1  #Superkey + 1 switches to workspace 1
bind=SUPER,1,exec,$w1     #SuperKey + 1 switches to wallpaper $w1 on DP-1 as defined in the variable

bind=SUPER,2,workspace,2  #Superkey + 2 switches to workspace 2
bind=SUPER,2,exec,$w2     #SuperKey + 2 switches to wallpaper $w2 on DP-1 as defined in the variable

bind=SUPER,3,workspace,3  #Superkey + 3 switches to workspace 3
bind=SUPER,3,exec,$w3     #SuperKey + 3 switches to wallpaper $w3 on DP-1 as defined in the variable

#... and so on
```
Because the default behavior in Hyprland is to also switch the workspace whenever movetoworkspace is used to move a window to another workspace you may want to include the following:

```
bind=SUPERSHIFT,1,movetoworkspace,1  #Superkey + Shift + 1 moves windows and switches to workspace 1
bind=SUPERSHIFT,1,exec,$w1           #SuperKey + Shift + 1 switches to wallpaper $w1 on DP-1 as defined in the variable
```

## Getting information from hyprpaper
You can also use `hyprctl hyprpaper` to get information about the state of hyprpaper using the following commands:
```
listloaded - lists the wallpapers that are currently preloaded (useful for dynamically preloading and unloading)
listactive - prints the active wallpapers hyprpaper is displaying, along with its accociated monitor
```

# Battery life
Since the IPC has to tick every now and then, and poll in the background, battery life might be a tiny bit worse with IPC on. If you want to fully disable it, use
```
ipc = off
```
in the config.

# Misc
You can set `splash = true` to enable the splash rendering over the wallpaper.

The value for `splash_offset` sets, in percentage, the splash rendering offset relative to the bottom of the display.

## Unloading
If you use a lot of wallpapers, consider unloading those that you no longer need. This will mean you need to load them again if you wish to use them for a second time, but will free the memory used by the preloaded bitmap. (Usually 8 - 20MB, depending on the resolution)

You can issue a `hyprctl hyprpaper unload [PATH]` to do that.

You can also issue a `hyprctl hyprpaper unload all` to unload all inactive wallpapers.

<br/>

For other compositors, the socket works like socket1 of Hyprland, and is located in `/tmp/hypr/.hyprpaper.sock` (this path only when Hyprland is not running!)
