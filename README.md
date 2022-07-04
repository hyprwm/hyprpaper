# hyprpaper

Hyprpaper is a blazing fast wallpaper utility for Hyprland with the ability to dynamically change wallpapers through sockets. It will work on all wlroots-based compositors, though.

# Installation

```
git clone https://github.com/hyprwm/hyprpaper
make all
```
*the output binary will be in `./build/`, copy it to your PATH, e.g. `/usr/bin`*

# Usage

Hyprpaper is controlled by the config, like this:

*~/.config/hypr/hyprpaper.conf*
```
preload = /path/to/image.png

# .. more preloads

wallpaper = monitor,/path/to/image.png

# .. more monitors
```

Preload will tell Hyprland to load a particular image (supported formats: png, jpg, jpeg). Wallpaper will apply the wallpaper to the selected output (`monitor` is the monitor's name, easily can be retrieved with `hyprctl monitors`)

A Wallpaper ***cannot*** be applied without preloading. The config is ***not*** reloaded dynamically.

# IPC
You can use `hyprctl hyprpaper` (if on Hyprland) to issue a keyword, for example
```
hyprctl hyprpaper wallpaper DP-1,~/Pictures/myepicpng.png
```

For other compositors, the socket works like socket1 of Hyprland, and is located in `/tmp/hypr/.hyprpaper.sock` (this path only when Hyprland is not running!)
