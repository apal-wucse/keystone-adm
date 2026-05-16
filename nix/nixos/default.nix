{ inputs, system }:
let
  inherit (inputs.nixpkgs.lib) nixosSystem;
in
{
  virt = nixosSystem {
    specialArgs = {
      inherit inputs;
    };
    modules = [
      ./common.nix
      ./virt/configurations.nix
      {
        nixpkgs.buildPlatform = system;
      }
    ];
  };

  unmatched = nixosSystem {
    specialArgs = {
      inherit inputs;
    };
    modules = [
      ./common.nix
      ./unmatched/configurations.nix
      {
        nixpkgs.buildPlatform = system;
      }
    ];
  };
}
