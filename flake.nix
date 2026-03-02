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

  outputs =
    {
      self,
      nixpkgs,
      systems,
      ...
    }@inputs:
    let
      inherit (nixpkgs) lib;
      eachSystem = lib.genAttrs (import systems);

      pkgsFor = eachSystem (
        system:
        import nixpkgs {
          localSystem.system = system;
          overlays = with self.overlays; [ hyprpaper-with-deps ];
        }
      );
    in
    {
      overlays = import ./nix/overlays.nix { inherit inputs lib self; };

      packages = eachSystem (system: {
        default = self.packages.${system}.hyprpaper;
        inherit (pkgsFor.${system}) hyprpaper hyprpaper-debug;
      });

      homeManagerModules = {
        default = self.homeManagerModules.hyprpaper;
        hyprpaper = builtins.throw "hyprpaper: the flake HM module has been removed. Use the module from Home Manager upstream.";
      };

      formatter = eachSystem (system: pkgsFor.${system}.nixfmt-tree);
    };
}
