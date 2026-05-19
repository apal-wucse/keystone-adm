{
  lib,
  pkgs,
  nixos-virt,
  qemu,
  keystonePkgs,
}:
let
  kernelParams = lib.concatStringsSep " " (
    nixos-virt.config.boot.kernelParams
    ++ [
      "root=/dev/vda"
      "rw"
      "init=${nixos-virt.config.system.build.toplevel}/init"
    ]
  );
in
pkgs.writeShellApplication {
  name = "run-qemu-virt";
  runtimeInputs = [
    pkgs.coreutils
    qemu
  ];
  text = ''
    set -euo pipefail
    WORKDIR=''${WORKDIR:-./images}
    mkdir -p "$WORKDIR"

    if [ ! -f "$WORKDIR/nixos-virt-overlay.qcow2" ]; then
      qemu-img create -f qcow2 \
        -F qcow2 -b ${nixos-virt.config.system.build.qcow2}/nixos-virt.qcow2 \
        "$WORKDIR/nixos-virt-overlay.qcow2" 128G
    fi

    exec qemu-system-riscv64 \
      -machine virt,rom=${keystonePkgs.bootrom}/bin/bootrom.bin,acpi=off \
      -cpu rva23s64,pmp=true -smp cpus=4 -m 4G -nographic \
      -bios ${keystonePkgs.keystone-sm}/share/opensbi/lp64/generic/firmware/fw_jump.bin \
      -kernel ${nixos-virt.config.system.build.kernel}/Image \
      -initrd ${nixos-virt.config.system.build.initialRamdisk}/initrd \
      -append "${kernelParams}" \
      -drive file="$WORKDIR/nixos-virt-overlay.qcow2",format=qcow2,if=virtio \
      -netdev user,id=net0,hostfwd=tcp::2222-:22 \
      -device virtio-net-device,netdev=net0 \
      -object rng-random,filename=/dev/urandom,id=rng0 \
      -device virtio-rng-device,rng=rng0 
  '';
}
