{
  stdenv,
  cmake,
  glibc,
  mkKeystonePackage,
  keystonePkgs,
  withPlatform ? "generic",
}:
let
  runtime = keystonePkgs.runtime.override {
    inherit withPlatform;
    withEdgeProtection = false;
  };

  encApp = stdenv.mkDerivation (finalAttrs: {
    pname = "transfer-eapp";
    version = "0.1.0";
    src = ./eapp;

    buildInputs = [
      glibc.static
      keystonePkgs.keystone-sdk.bench
    ];

    nativeBuildInputs = [
      cmake
    ];

    cmakeFlags = [
      "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}"
    ];
  });

  hostApp = stdenv.mkDerivation (finalAttrs: {
    pname = "transfer-host";
    version = "0.1.0";
    src = ./host;

    buildInputs = [
      keystonePkgs.keystone-sdk.bench
    ];

    nativeBuildInputs = [
      cmake
    ];

    cmakeFlags = [
      "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}"
    ];
  });

in
mkKeystonePackage {
  pname = "transfer-ecall";
  version = "0.1.0";
  inherit encApp hostApp runtime;
}
