{ nixpkgs, system }:
let
  inherit (nixpkgs.lib) nixosSystem;
in
{
  virt = nixosSystem {
    modules = [
      ./virt/configurations.nix
      {
        nixpkgs.buildPlatform = system;
      }
    ];
  };
}
