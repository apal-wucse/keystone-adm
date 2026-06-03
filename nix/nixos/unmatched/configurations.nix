{
  config,
  lib,
  pkgs,
  modulesPath,
  ...
}:
let
  rootfsImage = import "${modulesPath}/../lib/make-ext4-fs.nix" {
    inherit pkgs lib;
    inherit (pkgs)
      e2fsprogs
      libfaketime
      perl
      fakeroot
      zstd
      ;
    storePaths = [ config.system.build.toplevel ];
    volumeLabel = "NIXOS_ROOT";
    compressImage = false;
  };
in
{
  hardware.deviceTree.name = "sifive/hifive-unmatched-a00.dtb";

  boot.loader.grub.enable = false;
  boot.loader.generic-extlinux-compatible.enable = true;

  boot.kernelParams = [
    "console=ttySIF0,115200"
    "earlycon=sbi"
  ];

  boot.initrd.kernelModules = [
    "nvme"
    "mmc_block"
    "mmc_spi"
    "spi_sifive"
    "spi_nor"
  ];

  boot.kernelPatches = [
    {
      name = "unmatched-config";
      patch = null;
      structuredExtraConfig = with lib.kernel; {
        SOC_SIFIVE = yes;
        PCIE_FU740 = yes;
        PWM_SIFIVE = yes;
        EDAC_SIFIVE = yes;
        SIFIVE_L2 = yes;
        RISCV_ERRATA_ALTERNATIVE = yes;
        ERRATA_SIFIVE = yes;
        ERRATA_SIFIVE_CIP_453 = yes;
        ERRATA_SIFIVE_CIP_1200 = yes;
      };
    }
    {
      name = "cpufreq";
      patch = null;
      structuredExtraConfig = with lib.kernel; {
        CPU_IDLE = yes;
        CPU_FREQ = yes;
        CPU_FREQ_DEFAULT_GOV_USERSPACE = yes;
        CPU_FREQ_GOV_PERFORMANCE = yes;
        CPU_FREQ_GOV_USERSPACE = yes;
        CPU_FREQ_GOV_ONDEMAND = yes;
      };
    }
  ];

  fileSystems."/" = {
    device = "/dev/disk/by-label/NIXOS_ROOT";
    autoResize = true;
    fsType = "ext4";
  };

  fileSystems."/boot" = {
    device = "/dev/disk/by-label/NIXOS_BOOT";
    fsType = "vfat";
    options = [
      "fmask=0077"
      "dmask=0077"
    ];
  };

  environment.systemPackages = with pkgs; [
    mtdutils
    (keystoneApps.override { withPlatform = "unmatched"; })
    (keystoneBenchmarks.override { withPlatform = "unmatched"; })
  ];

  system.build.rootfsImage = rootfsImage;
}
