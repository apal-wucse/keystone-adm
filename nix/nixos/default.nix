{ nixpkgs, system }:
let
  inherit (nixpkgs.lib) nixosSystem;
in
{
  virt = nixosSystem {
    modules = [
      ./common.nix
      ./virt/configurations.nix
      {
        nixpkgs.buildPlatform = system;
      }
    ];
  };

  unmatched = nixosSystem {
    modules = [
      ./common.nix
      ./unmatched/configurations.nix
      {
        nixpkgs.buildPlatform = system;
      }
    ];
  };
}
