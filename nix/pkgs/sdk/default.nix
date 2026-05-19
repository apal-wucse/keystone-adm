{
  lib,
  stdenv,
  cmake,
  clang-tools,
  cpplint,
  which,
  pkgsMusl,
}:
let
  mkKeystoneSdk =
    {
      withBenchmark ? false,
    }:
    stdenv.mkDerivation (finalAttrs: {
      pname = "keystone-sdk";
      version = "1.0.0-adm" + (if withBenchmark then "-benchmark" else "");
      src = ../../../sdk;

      nativeBuildInputs = [
        cmake
        clang-tools
        cpplint
        which
      ];

      cmakeFlags = [
        "-DKEYSTONE_SDK_DIR=${placeholder "out"}"
        "-DCROSS_COMPILE=${stdenv.cc.targetPrefix}"
      ]
      ++ lib.optional withBenchmark [
        "-DTIME_BENCHMARK=ON"
        "-DCS_BENCHMARK=ON"
      ];
    });

  keystoneSdk = mkKeystoneSdk { };
in
keystoneSdk
// {
  passthru = {
    bench = mkKeystoneSdk { withBenchmark = true; };
    musl = keystoneSdk.override {
      inherit (pkgsMusl) stdenv;
    };
  };
}
