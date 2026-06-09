{
  symlinkJoin,
  callPackage,
  withPlatform ? "generic",
}:
let
  ecall = callPackage ./ecall/package.nix { inherit withPlatform; };
  adm = callPackage ./adm/package.nix { inherit withPlatform; };
in
symlinkJoin {
  pname = "transfer-benchmarks";
  version = "0.1.0";

  paths = [
    ecall
    adm
  ];
}
