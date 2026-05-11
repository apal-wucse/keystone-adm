{
  self,
  pkgsRiscv64,
  system,
}:
{
  keystone-sm = pkgsRiscv64.callPackage ./keystone-sm.nix {
    keystone-sm = self.packages.${system}.keystone-sm.default;
  };

  keystone-sdk = pkgsRiscv64.callPackage ./sdk.nix {
    keystone-sdk = self.packages.${system}.keystone-sdk.default;
  };

  runtime = pkgsRiscv64.callPackage ./runtime.nix {
    runtime = self.packages.${system}.runtime.default;
    keystone-sdk = self.packages.${system}.keystone-sdk.default;
  };

  driver = pkgsRiscv64.callPackage ./driver.nix {
    keystone-driver = self.packages.${system}.driver;
  };

  bootrom = pkgsRiscv64.callPackage ./bootrom.nix {
    bootrom = self.packages.${system}.bootrom;
  };
}
