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
      in
      {
        packages = {
          keystone-sm = {
            default = pkgsRiscv64.callPackage ./nix/pkgs/keystone-sm.nix { };
            unmatched = pkgsRiscv64.callPackage ./nix/pkgs/keystone-sm.nix {
              withKeystonePlatform = "unmatched";
            };
          };
          keystone-sdk = {
            default = pkgsRiscv64.callPackage ./nix/pkgs/sdk.nix { };
            musl = pkgsRiscv64Musl.callPackage ./nix/pkgs/sdk.nix { };
            bench = pkgsRiscv64.callPackage ./nix/pkgs/sdk.nix { withBenchmark = true; };
            musl-bench = pkgsRiscv64Musl.callPackage ./nix/pkgs/sdk.nix { withBenchmark = true; };
          };
          runtime = {
            default = pkgsRiscv64.callPackage ./nix/pkgs/runtime.nix {
              keystone-sdk = self.packages.${system}.keystone-sdk.default;
              withFreeMem = true;
              withLinuxSyscall = true;
              withIoSyscall = true;
              withNetSyscall = true;
              withEdgeProtection = true;
              withGlibc = true;
            };
            nolibc = pkgsRiscv64.callPackage ./nix/pkgs/runtime.nix {
              keystone-sdk = self.packages.${system}.keystone-sdk.default;
              withFreeMem = true;
              withEdgeProtection = true;
            };
            musl = pkgsRiscv64Musl.callPackage ./nix/pkgs/runtime.nix {
              keystone-sdk = self.packages.${system}.keystone-sdk.musl;
              withFreeMem = true;
              withLinuxSyscall = true;
              withIoSyscall = true;
              withNetSyscall = true;
              withEdgeProtection = true;
              withGlibc = true;
            };
            musl-nolibc = pkgsRiscv64Musl.callPackage ./nix/pkgs/runtime.nix {
              keystone-sdk = self.packages.${system}.keystone-sdk.musl;
              withFreeMem = true;
              withEdgeProtection = true;
            };
          };
        };

        devShells = {
          keystone-sm = pkgsRiscv64.callPackage ./nix/shells/keystone-sm.nix {
            keystone-sm = self.packages.${system}.keystone-sm.default;
          };
          keystone-sdk = pkgsRiscv64.callPackage ./nix/shells/sdk.nix {
            keystone-sdk = self.packages.${system}.keystone-sdk.default;
          };
          runtime = pkgsRiscv64.callPackage ./nix/shells/runtime.nix {
            runtime = self.packages.${system}.runtime.default;
            keystone-sdk = self.packages.${system}.keystone-sdk.default;
          };
        };

        formatter = treefmt-nix.lib.mkWrapper pkgs {
          projectRootFile = "flake.nix";
          programs.nixfmt.enable = true;
        };
      }
    );
}
