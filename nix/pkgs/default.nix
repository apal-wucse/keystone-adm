{
  self,
  pkgs,
  pkgsRiscv64,
  pkgsRiscv64Musl,
  system,
  nixosSystems,
}:
rec {
  keystone-sm = {
    default = pkgsRiscv64.callPackage ./keystone-sm { };
    unmatched = pkgsRiscv64.callPackage ./keystone-sm {
      withKeystonePlatform = "unmatched";
    };
  };

  keystone-sdk = {
    default = pkgsRiscv64.callPackage ./sdk { };
    musl = pkgsRiscv64Musl.callPackage ./sdk { };
    bench = pkgsRiscv64.callPackage ./sdk { withBenchmark = true; };
    musl-bench = pkgsRiscv64Musl.callPackage ./sdk { withBenchmark = true; };
  };

  runtime = {
    default = pkgsRiscv64.callPackage ./runtime {
      keystone-sdk = keystone-sdk.default;
      withFreeMem = true;
      withLinuxSyscall = true;
      withIoSyscall = true;
      withNetSyscall = true;
      withEdgeProtection = true;
      withGlibc = true;
    };

    nolibc = pkgsRiscv64.callPackage ./runtime {
      keystone-sdk = keystone-sdk.default;
      withFreeMem = true;
      withEdgeProtection = true;
    };

    musl = pkgsRiscv64Musl.callPackage ./runtime {
      keystone-sdk = keystone-sdk.musl;
      withFreeMem = true;
      withLinuxSyscall = true;
      withIoSyscall = true;
      withNetSyscall = true;
      withEdgeProtection = true;
      withGlibc = true;
    };

    musl-nolibc = pkgsRiscv64Musl.callPackage ./runtime {
      keystone-sdk = keystone-sdk.musl;
      withFreeMem = true;
      withEdgeProtection = true;
    };
  };

  driver = pkgsRiscv64.linuxPackages.callPackage ./driver { };

  qemu = pkgs.callPackage ./qemu { };

  meta-sifive = pkgs.callPackage ./meta-sifive { };

  bootloader = {
    bootrom = pkgsRiscv64.callPackage ./bootrom { };
    u-boot = {
      unmatched = pkgsRiscv64.callPackage ./u-boot {
        inherit meta-sifive;
        keystone-sm = keystone-sm.unmatched;
        defconfig = "sifive_unmatched_keystone_defconfig";
      };
    };
  };

  image = {
    virt = nixosSystems.virt.config.system.build.qcow2;
    unmatched = pkgs.callPackage ./unmatched-sd-image {
      u-boot-keystone = bootloader.u-boot.unmatched;
      nixosSystem = nixosSystems.unmatched;
    };
    unmatched-root = nixosSystems.unmatched.config.system.build.rootfsImage;
  };

  run-virt = pkgs.callPackage ./run-virt {
    nixos-virt = nixosSystems.virt;
    qemu-keystone = qemu;
    keystone-bootrom = bootloader.bootrom;
    keystone-sm = keystone-sm.default;
  };
}
