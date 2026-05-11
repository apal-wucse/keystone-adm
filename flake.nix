{
  description = "Keystone Enclave with Additional Data Memory";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-25.11";
    flake-utils.url = "github:numtide/flake-utils";
    flake-compat.url = "github:edolstra/flake-compat";
    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
      treefmt-nix,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };

        pkgsRiscv64 = import nixpkgs {
          inherit system;
          crossSystem = {
            config = "riscv64-unknown-linux-gnu";
          };
        };

        pkgsRiscv64Musl = import nixpkgs {
          inherit system;
          crossSystem = {
            config = "riscv64-unknown-linux-musl";
          };
        };

        nixosSystems = import ./nix/nixos {
          inherit
            nixpkgs
            system
            ;
        };
      in
      {
        packages = import ./nix/pkgs {
          inherit
            self
            pkgs
            pkgsRiscv64
            pkgsRiscv64Musl
            system
            nixosSystems
            ;
        };

        devShells = import ./nix/shells {
          inherit
            self
            pkgsRiscv64
            system
            ;
        };

        formatter = treefmt-nix.lib.mkWrapper pkgs {
          projectRootFile = "flake.nix";
          programs.nixfmt.enable = true;
        };
      }
    );
}
