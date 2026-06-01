{
  symlinkJoin,
  callPackage,
  withPlatform ? "generic",
}:
let
  hello = callPackage ../../../apps/hello/package.nix { inherit withPlatform; };
  hello-native = callPackage ../../../apps/hello-native/package.nix { inherit withPlatform; };
  iotest = callPackage ../../../apps/iotest/package.nix { inherit withPlatform; };
  iotest-adm = callPackage ../../../apps/iotest-adm/package.nix { inherit withPlatform; };
  iotest-adm-nocpy = callPackage ../../../apps/iotest-adm-nocpy/package.nix { inherit withPlatform; };
  admtest = callPackage ../../../apps/admtest/package.nix { inherit withPlatform; };
in
symlinkJoin {
  pname = "keystone-apps";
  version = "0.1.0";

  paths = [
    hello
    hello.adm
    hello-native
    iotest
    iotest-adm
    iotest-adm-nocpy
    admtest
  ];
}
