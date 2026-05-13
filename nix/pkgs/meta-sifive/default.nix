{ fetchFromGitHub }:
rec {
  src = fetchFromGitHub {
    owner = "sifive";
    repo = "meta-sifive";
    tag = "2023.08.01";
    sha256 = "sha256-Lug8X9qA29Qz/RwCIPYxIR1DF0Ko/h4x5NdsERuAl5Y=";
  };

  ubootPatches = map (patch: "${src}/recipes-bsp/u-boot/files/riscv64/${patch}") [
    "0002-board-sifive-spl-Initialized-the-PWM-setting-in-the-.patch"
    "0003-board-sifive-Set-LED-s-color-to-purple-in-the-U-boot.patch"
    "0004-board-sifive-Set-LED-s-color-to-blue-before-jumping-.patch"
    "0005-board-sifive-spl-Set-remote-thermal-of-TMP451-to-85-.patch"
    "0006-riscv-sifive-fu740-reduce-DDR-speed-from-1866MT-s-to.patch"
  ];
}
