{
  symlinkJoin,
  callPackage,
  withPlatform ? "generic",
}:
let
  cswitch = callPackage ../../../benchmarks/cswitch/package.nix { inherit withPlatform; };
  transfer = callPackage ../../../benchmarks/transfer/package.nix { inherit withPlatform; };
in
symlinkJoin {
  pname = "keystone-benchmarks";
  version = "0.1.0";

  paths = [
    cswitch
    transfer
  ];
}
