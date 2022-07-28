{
  description = "Hyprpaper is a blazing fast Wayland wallpaper utility with IPC controls";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = {
    self,
    nixpkgs,
    ...
  }: let
    inherit (nixpkgs) lib;
    genSystems = lib.genAttrs [
      # Add more systems if they are supported
      "x86_64-linux"
    ];
    pkgsFor = nixpkgs.legacyPackages;
    mkDate = longDate: (lib.concatStringsSep "-" [
      (__substring 0 4 longDate)
      (__substring 4 2 longDate)
      (__substring 6 2 longDate)
    ]);
  in {
    overlays.default = _: prev: rec {
      hyprpaper = prev.callPackage ./nix/default.nix {
        stdenv = prev.gcc12Stdenv;
        version = "0.pre" + "+date=" + (mkDate (self.lastModifiedDate or "19700101")) + "_" + (self.shortRev or "dirty");
        inherit (prev.xorg) libXdmcp;
      };
      hyprpaper-debug = hyprpaper.override {debug = true;};
    };

    packages = genSystems (system:
      (self.overlays.default null pkgsFor.${system})
      // {default = self.packages.${system}.hyprpaper;});

    formatter = genSystems (system: pkgsFor.${system}.alejandra);
  };
}
