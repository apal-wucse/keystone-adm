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
    volumeLabel = "NIXOS";
    compressImage = false;
  };

  qcowImage =
    pkgs.runCommand "nixos-qcow2" { nativeBuildInputs = [ pkgs.buildPackages.qemu-utils ]; }
      ''
        mkdir -p $out
        qemu-img convert -f raw -O qcow2 ${rootfsImage} $out/nixos-virt.qcow2
      '';

  keystone-driver = config.boot.kernelPackages.callPackage ../../pkgs/driver.nix { };
in
{
  imports = [ "${modulesPath}/profiles/qemu-guest.nix" ];

  nixpkgs.hostPlatform = "riscv64-linux";

  boot.loader.grub.enable = false;
  boot.loader.generic-extlinux-compatible.enable = false;

  boot.kernelPackages = pkgs.linuxPackages_6_12;

  boot.kernelParams = [
    "console=ttyS0,115200"
    "earlycon=sbi"
  ];

  boot.kernelPatches = [
    {
      name = "cma";
      patch = null;
      structuredExtraConfig = with lib.kernel; {
        CMA = yes;
        CMA_SIZE_SEL_MBYTES = yes;
        CMA_SIZE_MBYTES = freeform "1024";
        CMA_ALIGNMENT = freeform "9";
        CMA_DEBUG = yes;
        CMA_DEBUGFS = yes;
        CMA_SYSFS = yes;
        DMA_CMA = yes;
      };
    }
  ];

  boot.extraModulePackages = [ keystone-driver ];
  boot.kernelModules = [ "keystone-driver" ];

  fileSystems."/" = {
    device = "/dev/disk/by-label/NIXOS";
    autoResize = true;
    fsType = "ext4";
  };

  services.openssh.enable = true;
  services.openssh.settings.PermitRootLogin = "yes";
  networking.firewall.allowedTCPPorts = [ 22 ];

  users.users.root.initialPassword = "nixos";
  users.users.nixos = {
    isNormalUser = true;
    extraGroups = [ "wheel" ];
    initialPassword = "nixos";
  };

  environment.systemPackages = with pkgs; [
    vim
    git
    tmux
    htop
    pciutils
    usbutils
  ];

  system.build.qcow2 = qcowImage;

  system.stateVersion = "25.11";
}
