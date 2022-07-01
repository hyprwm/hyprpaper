# hyprpaper

Hyprpaper is a blazing fast wallpaper utility for Hyprland with the ability to dynamically change wallpapers through sockets*. It will work on all wlroots-based compositors, though.

*todo

# Installation

```
git clone https://github.com/hyprwm/hyprpaper
make all
```
*the output binary will be in `./build/`*

# Usage

Hyprpaper is controlled by the config, like this:

*~/.config/hypr/hyprpaper.conf*
```
preload = /path/to/image.png

# .. more preloads

wallpaper = monitor,/path/to/image.png

# .. more monitors
```

Preload will tell Hyprland to load a particular image. Wallpaper will apply the wallpaper to the selected output (`monitor` is the monitor's name, easily can be retrieved with `hyprctl monitors`)

A Wallpaper ***cannot*** be applied without preloading. The config is ***not*** reloaded dynamically.

# todos

- socket communication (hyprctl)
- switching wps
- allow setting only for selected monitors
