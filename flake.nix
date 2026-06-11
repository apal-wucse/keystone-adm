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
    }@inputs:
    flake-utils.lib.eachSystem
      [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
        "riscv64-linux"
      ]
      (
        system:
        let
          keystoneOverlay = import ./nix/pkgs/riscv-overlay.nix;

          pkgs = import nixpkgs {
            inherit system;
          };

          pkgsRiscv64 = import nixpkgs {
            inherit system;
            crossSystem = {
              config = "riscv64-unknown-linux-gnu";
            };
            overlays = [ keystoneOverlay ];
          };

          nixosSystems = import ./nix/nixos {
            inherit
              inputs
              system
              keystoneOverlay
              ;
          };
        in
        {
          packages =
            import ./nix/pkgs {
              inherit
                self
                pkgs
                system
                nixosSystems
                ;
              inherit (pkgsRiscv64) keystonePkgs;
            }
            // {
              inherit (pkgsRiscv64.linuxPackages)
                keystone-driver
                ;
              inherit (pkgsRiscv64.keystonePkgs)
                bootrom
                u-boot
                keystone-sm
                keystone-sdk
                runtime
                ;
              inherit (pkgsRiscv64)
                keystoneApps
                keystoneBenchmarks
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
            programs = {
              nixfmt.enable = true;
              clang-format.enable = true;
            };
            settings.formatter.clang-format.excludes = [
              "benchmarks/iozone/src/**"
              "crypto/**"
              "runtime/crypto/**"
              "runtime/include/crypto/**"
              "sdk/**/common/**"
              "sdk/**/json11.cpp"
              "sdk/**/json11.h"
              "sdk/**/verifier/ed25519/**"
              "sdk/src/app-tmalloc/**"
              "sm/src/ed25519/**"
              "sm/src/hkdf_sha3_512/**"
              "sm/src/hmac_sha3/**"
              "sm/src/sha3/**"
            ];
          };
        }
      );
}
