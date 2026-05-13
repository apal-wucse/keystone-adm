{
  runCommand,
  buildPackages,
  u-boot-keystone,
  nixosSystem,
  imageSize ? "4G",
  label ? "unmatched",
}:
let
  inherit (nixosSystem.config.system.build) rootfsImage;
in
runCommand "nixos-sd-image-${label}.img"
  {
    nativeBuildInputs = with buildPackages; [
      parted
      gptfdisk
      util-linux
      coreutils
      mtools
      dosfstools
    ];
  }
  ''
    set -euo pipefail

    img=$out

    truncate -s ${imageSize} "$img"

    sgdisk -og -a 1\
      --new=1:34:2081 --change-name=1:spl \
      --typecode=1:5B193300-FC78-40CD-8002-E86C45580B47 \
      --new=2:2082:10273 --change-name=2:uboot \
      --typecode=2:2E54B353-1271-4842-806F-E436D6AF6985 \
      --new=3:16384:282623 --change-name=3:NIXOS_BOOT \
      --typecode=3:EBD0A0A2-B9E5-4433-87C0-68B6B72699C7 \
      --new=4:286720:0 --change-name=4:NIXOS_ROOT \
      --typecode=4:0FC63DAF-8483-4772-8E79-3D69D8477DE4 \
      "$img"

    ${nixosSystem.config.boot.loader.generic-extlinux-compatible.populateCmd} \
      -c ${nixosSystem.config.system.build.toplevel} \
      -d ./files/boot

    truncate -s 130M boot.img
    mkfs.vfat -F32 boot.img
    mcopy -i boot.img -s files/boot/* ::/

    dd if=${u-boot-keystone}/u-boot-spl.bin of="$img" \
       bs=512 seek=34 conv=notrunc status=progress

    dd if=${u-boot-keystone}/u-boot.itb of="$img" \
       bs=512 seek=2082 conv=notrunc status=progress

    dd if=boot.img of="$img" \
       bs=512 seek=16384 conv=notrunc status=progress

    echo "Using rootfs image: ${rootfsImage}"
    dd if="${rootfsImage}" of="$img" \
       bs=512 seek=286720 \
       conv=notrunc,sparse status=progress
  ''
