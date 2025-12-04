{
  description = "Hyprpaper is a blazing fast Wayland wallpaper utility with IPC controls";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    systems.url = "github:nix-systems/default-linux";

    aquamarine = {
      url = "github:hyprwm/aquamarine";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
      inputs.hyprwayland-scanner.follows = "hyprwayland-scanner";
    };

    hyprgraphics = {
      url = "github:hyprwm/hyprgraphics";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    hyprutils = {
      url = "github:hyprwm/hyprutils";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    hyprlang = {
      url = "github:hyprwm/hyprlang";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    hyprwayland-scanner = {
      url = "github:hyprwm/hyprwayland-scanner";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
    };

    hyprwire = {
      url = "github:hyprwm/hyprwire";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.hyprutils.follows = "hyprutils";
    };

    hyprtoolkit = {
      url = "github:hyprwm/hyprtoolkit";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.systems.follows = "systems";
      inputs.aquamarine.follows = "aquamarine";
      inputs.hyprutils.follows = "hyprutils";
      inputs.hyprlang.follows = "hyprlang";
      inputs.hyprgraphics.follows = "hyprgraphics";
      inputs.hyprwayland-scanner.follows = "hyprwayland-scanner";
    };
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
    version = lib.removeSuffix "\n" (builtins.readFile ./VERSION);
  in {
    overlays = {
      default = self.overlays.hyprpaper;
      hyprpaper = lib.composeManyExtensions [
        inputs.aquamarine.overlays.default
        inputs.hyprgraphics.overlays.default
        inputs.hyprlang.overlays.default
        inputs.hyprutils.overlays.default
        inputs.hyprwayland-scanner.overlays.default
        inputs.hyprtoolkit.overlays.default
        inputs.hyprwire.overlays.default
        (final: prev: rec {
          hyprpaper = final.callPackage ./nix/default.nix {
            stdenv = final.gcc15Stdenv;
            version = version + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
            commit = self.rev or "";
          };
          hyprpaper-debug = hyprpaper.override {debug = true;};
        })
      ];
    };

    packages = eachSystem (system: {
      default = self.packages.${system}.hyprpaper;
      inherit (pkgsFor.${system}) hyprpaper hyprpaper-debug;
    });

    homeManagerModules = {
      default = self.homeManagerModules.hyprpaper;
      hyprpaper = builtins.throw "hyprpaper: the flake HM module has been removed. Use the module from Home Manager upstream.";
    };

    formatter = eachSystem (system: pkgsFor.${system}.alejandra);
  };
}
