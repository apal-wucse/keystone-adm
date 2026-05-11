{
  self,
  pkgs,
  pkgsRiscv64,
  pkgsRiscv64Musl,
  system,
  nixosSystems,
}:
{
  keystone-sm = {
    default = pkgsRiscv64.callPackage ./keystone-sm.nix { };
    unmatched = pkgsRiscv64.callPackage ./keystone-sm.nix {
      withKeystonePlatform = "unmatched";
    };
  };

  keystone-sdk = {
    default = pkgsRiscv64.callPackage ./sdk.nix { };
    musl = pkgsRiscv64Musl.callPackage ./sdk.nix { };
    bench = pkgsRiscv64.callPackage ./sdk.nix { withBenchmark = true; };
    musl-bench = pkgsRiscv64Musl.callPackage ./sdk.nix { withBenchmark = true; };
  };

  runtime = {
    default = pkgsRiscv64.callPackage ./runtime.nix {
      keystone-sdk = self.packages.${system}.keystone-sdk.default;
      withFreeMem = true;
      withLinuxSyscall = true;
      withIoSyscall = true;
      withNetSyscall = true;
      withEdgeProtection = true;
      withGlibc = true;
    };

    nolibc = pkgsRiscv64.callPackage ./runtime.nix {
      keystone-sdk = self.packages.${system}.keystone-sdk.default;
      withFreeMem = true;
      withEdgeProtection = true;
    };

    musl = pkgsRiscv64Musl.callPackage ./runtime.nix {
      keystone-sdk = self.packages.${system}.keystone-sdk.musl;
      withFreeMem = true;
      withLinuxSyscall = true;
      withIoSyscall = true;
      withNetSyscall = true;
      withEdgeProtection = true;
      withGlibc = true;
    };

    musl-nolibc = pkgsRiscv64Musl.callPackage ./runtime.nix {
      keystone-sdk = self.packages.${system}.keystone-sdk.musl;
      withFreeMem = true;
      withEdgeProtection = true;
    };
  };

  driver = pkgsRiscv64.linuxPackages.callPackage ./driver.nix { };
  bootrom = pkgsRiscv64.callPackage ./bootrom.nix { };
  qemu = pkgs.callPackage ./qemu.nix { };

  image = {
    virt = nixosSystems.virt.config.system.build.qcow2;
  };

  run-virt = pkgs.callPackage ./run-virt.nix {
    nixos-virt = nixosSystems.virt;
    qemu-keystone = self.packages.${system}.qemu;
    keystone-bootrom = self.packages.${system}.bootrom;
    keystone-sm = self.packages.${system}.keystone-sm.default;
  };
}
