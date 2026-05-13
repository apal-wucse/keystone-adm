{ pkgs }:
pkgs.qemu.overrideAttrs (
  final: prev: {
    version = "11.0.0";
    pname = "qemu-riscv64-keystone";
    src = pkgs.fetchurl {
      url = "https://download.qemu.org/qemu-${final.version}.tar.xz";
      hash = "sha256-wEyjYBJlPzLRHGdNNwz1KnEOfT8Ywti2PkkyBSpIVNY=";
    };
    depsBuildBuild =
      prev.depsBuildBuild
      ++ (with pkgs; [
        python3Packages.setuptools
        python3Packages.wheel
      ]);
    checkInputs = with pkgs; [
      python3Packages.pygdbmi
      python3Packages.qemu-qmp
    ];
    patches = (prev.patches or [ ]) ++ [
      ../../patches/qemu/qemu-bootrom.patch
    ];
    configureFlags =
      (pkgs.lib.filter (x: !(pkgs.lib.hasPrefix "--target-list=" x)) prev.configureFlags)
      ++ [ "--target-list=riscv64-softmmu" ];
    postInstall = pkgs.lib.pipe (prev.postInstall or "") [
      (pkgs.lib.replaceStrings [ "ln -s $out/bin/qemu-system-x86_64 $out/bin/qemu-kvm" ] [ "" ])
    ];
  }
)
