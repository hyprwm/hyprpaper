{
  lib,
  inputs,
  self,
  ...
}:
let
  mkDate =
    longDate:
    (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);
  version = lib.removeSuffix "\n" (builtins.readFile ../VERSION);
in

{
  default = self.overlays.hyprpaper;

  hyprpaper-with-deps = lib.composeManyExtensions [
    inputs.aquamarine.overlays.default
    inputs.hyprgraphics.overlays.default
    inputs.hyprlang.overlays.default
    inputs.hyprutils.overlays.default
    inputs.hyprwayland-scanner.overlays.default
    inputs.hyprtoolkit.overlays.default
    inputs.hyprwire.overlays.default
    self.overlays.hyprpaper
  ];

  hyprpaper = final: prev: rec {
    hyprpaper = final.callPackage ./default.nix {
      stdenv = final.gcc15Stdenv;
      version =
        version
        + "+date="
        + (mkDate (self.lastModifiedDate or "19700101"))
        + "_"
        + (self.shortRev or "dirty");
      commit = self.rev or "";
    };
    hyprpaper-debug = hyprpaper.override { debug = true; };
  };
}
