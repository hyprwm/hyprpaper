{
  description = "Hyprpaper is a blazing fast Wayland wallpaper utility with IPC controls";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    hyprlang.url = "github:hyprwm/hyprlang";

    systems.url = "github:nix-systems/default-linux";
  };

  outputs = {
    self,
    nixpkgs,
    systems,
    ...
  } @ inputs: let
    inherit (nixpkgs) lib;
    eachSystem = lib.genAttrs (import systems);

    pkgsFor = eachSystem (system:
      import nixpkgs {
        localSystem.system = system;
        overlays = with self.overlays; [hyprpaper];
      });
    mkDate = longDate: (lib.concatStringsSep "-" [
      (builtins.substring 0 4 longDate)
      (builtins.substring 4 2 longDate)
      (builtins.substring 6 2 longDate)
    ]);
  in {
    overlays = {
      default = self.overlays.hyprpaper;
      hyprpaper = final: prev: rec {
        hyprpaper = final.callPackage ./nix/default.nix {
          stdenv = final.gcc13Stdenv;
          version = "0.pre" + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
          commit = self.rev or "";
          inherit (final.xorg) libXdmcp;
          inherit (inputs.hyprlang.packages.${final.system}) hyprlang;
        };
        hyprpaper-debug = hyprpaper.override {debug = true;};
      };
    };

    packages = eachSystem (system: {
      default = self.packages.${system}.hyprpaper;
      inherit (pkgsFor.${system}) hyprpaper hyprpaper-debug;
    });

    homeManagerModules = {
      default = self.homeManagerModules.hyprpaper;
      hyprpaper = import ./nix/hm-module.nix self;
    };

    formatter = eachSystem (system: pkgsFor.${system}.alejandra);
  };
}
