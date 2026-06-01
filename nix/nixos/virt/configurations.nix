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
in
{
  imports = [ "${modulesPath}/profiles/qemu-guest.nix" ];

  boot.loader.grub.enable = false;
  boot.loader.generic-extlinux-compatible.enable = false;

  boot.kernelParams = [
    "console=ttyS0,115200"
    "earlycon=sbi"
  ];

  fileSystems."/" = {
    device = "/dev/disk/by-label/NIXOS";
    autoResize = true;
    fsType = "ext4";
  };

  environment.systemPackages = [
    pkgs.keystoneApps
  ];

  system.build.qcow2 = qcowImage;
}
