{
  buildUBoot,
  fetchurl,
  keystonePkgs,
  meta-sifive,
  defconfig ? "sifive_unmatched_keystone_defconfig",
}:
let
  keystone-sm = keystonePkgs.keystone-sm.override { withKeystonePlatform = "unmatched"; };
in
buildUBoot rec {
  version = "2023.07.02";

  src = fetchurl {
    url = "https://ftp.denx.de/pub/u-boot/u-boot-${version}.tar.bz2";
    hash = "sha256-a2pIWBwUq7D5W9h8GvTXQJIkBte4AQAqn5Ryf93gIdU=";
  };

  prePatch = ''
    cp -r --no-preserve=mode,ownership ${../../../bootloader/u-boot}/. ./
    cp -r --no-preserve=mode,ownership ${../../../crypto}/. ./arch/riscv/lib/keystone/
  '';

  inherit defconfig;

  extraMeta.platforms = [ "riscv64-linux" ];

  extraPatches =
    meta-sifive.ubootPatches
    ++ map (patch: "${../../patches/u-boot}/${patch}") [
      "0005-fu740-pmu.patch"
      "0006-implement-keystone-secure-boot.patch"
      "fix-implicit-declaration.patch"
    ];

  extraMakeFlags = [
    "OPENSBI=${keystone-sm}/share/opensbi/lp64/generic/firmware/fw_dynamic.bin"
  ];

  filesToInstall = [
    "u-boot.itb"
    "u-boot"
    "spl/u-boot-spl.bin"
    "spl/u-boot-spl"
  ];
}
