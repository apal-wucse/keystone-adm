{
  self,
  pkgsRiscv64,
  system,
}:
{
  keystone-sm = pkgsRiscv64.callPackage ./keystone-sm {
    keystone-sm = self.packages.${system}.keystone-sm.default;
  };

  keystone-sdk = pkgsRiscv64.callPackage ./sdk {
    keystone-sdk = self.packages.${system}.keystone-sdk.default;
  };

  runtime = pkgsRiscv64.callPackage ./runtime {
    runtime = self.packages.${system}.runtime.default;
    keystone-sdk = self.packages.${system}.keystone-sdk.default;
  };

  driver = pkgsRiscv64.callPackage ./driver {
    keystone-driver = self.packages.${system}.driver;
  };

  bootrom = pkgsRiscv64.callPackage ./bootrom {
    bootrom = self.packages.${system}.bootrom;
  };
}
