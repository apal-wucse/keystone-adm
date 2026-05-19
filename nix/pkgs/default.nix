{
  self,
  pkgs,
  keystonePkgs,
  system,
  nixosSystems,
}:
rec {
  qemu = pkgs.callPackage ./qemu { };

  image = {
    virt = nixosSystems.virt.config.system.build.qcow2;
    unmatched = pkgs.callPackage ./unmatched-sd-image {
      inherit keystonePkgs;
      nixosSystem = nixosSystems.unmatched;
    };
    unmatched-root = nixosSystems.unmatched.config.system.build.rootfsImage;
  };

  run-virt = pkgs.callPackage ./run-virt {
    inherit keystonePkgs qemu;
    nixos-virt = nixosSystems.virt;
  };
}
