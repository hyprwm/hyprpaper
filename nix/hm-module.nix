self: {
  config,
  pkgs,
  lib,
  ...
}: let
  inherit (builtins) toString;
  inherit (lib.types) bool float listOf package str;
  inherit (lib.modules) mkIf;
  inherit (lib.options) mkOption mkEnableOption;
  inherit (lib.meta) getExe;

  boolToString = x:
    if x
    then "true"
    else "false";
  cfg = config.services.hyprpaper;
in {
  options.services.hyprpaper = {
    enable = mkEnableOption "Hyprpaper, Hyprland's wallpaper utility";

    package = mkOption {
      description = "The hyprpapr package";
      type = package;
      default = self.packages.${pkgs.stdenv.hostPlatform.system}.hyprpaper;
    };

    ipc = mkOption {
      description = "Whether to enable IPC";
      type = bool;
      default = true;
    };

    splash = mkOption {
      description = "Enable rendering of the hyprland splash over the wallpaper";
      type = bool;
      default = false;
    };

    splash_offset = mkOption {
      description = "How far (in % of height) up should the splash be displayed";
      type = float;
      default = 2.0;
    };

    preloads = mkOption {
      description = "List of paths to images that will be loaded into memory.";
      type = listOf str;
      example = [
        "~/Images/wallpapers/forest.png"
        "~/Images/wallpapers/desert.png"
      ];
    };

    wallpapers = mkOption {
      description = "The wallpapers";
      type = listOf str;
      example = [
        "eDP-1,~/Images/wallpapers/forest.png"
        "DP-7,~/Images/wallpapers/desert.png"
      ];
    };
  };

  config = mkIf cfg.enable {
    xdg.configFile."hypr/hyprpaper.conf".text = ''
      ipc = ${
        if cfg.ipc
        then "on"
        else "off"
      }
      splash = ${boolToString cfg.splash}
      splash_offset = ${toString cfg.splash_offset}

      ${
        builtins.concatStringsSep "\n"
        (
          map (preload: "preload = ${preload}") cfg.preloads
        )
      }
      ${
        builtins.concatStringsSep "\n"
        (
          map (wallpaper: "wallpaper = ${wallpaper}") cfg.wallpapers
        )
      }
    '';

    systemd.user.services.hyprpaper = {
      Unit = {
        Description = "Hyprland wallpaper daemon";
        PartOf = ["graphical-session.target"];
      };

      Service = {
        ExecStart = "${getExe cfg.package}";
        Restart = "on-failure";
      };
      Install.WantedBy = ["graphical-session.target"];
    };
  };
}
