{
  self,
  pkgsRiscv64,
  system,
}:
{
  keystone-sm = pkgsRiscv64.callPackage ./keystone-sm { };
  keystone-sdk = pkgsRiscv64.callPackage ./sdk { };
  runtime = pkgsRiscv64.callPackage ./runtime { };
  driver = pkgsRiscv64.callPackage ./driver { };
  bootrom = pkgsRiscv64.callPackage ./bootrom { };
  apps = pkgsRiscv64.callPackage ./apps { };
}
