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
    withLinuxSyscall = false;
    withIoSyscall = false;
    withNetSyscall = false;
    withGlibc = false;
  };

  encApp = stdenv.mkDerivation (finalAttrs: {
    pname = "keystone-hello-native-eapp";
    version = "0.1.0";
    src = ./eapp;

    buildInputs = [
      glibc.static
      keystonePkgs.keystone-sdk
    ];

    nativeBuildInputs = [
      cmake
    ];

    cmakeFlags = [
      "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}"
    ];
  });

  hostApp = stdenv.mkDerivation (finalAttrs: {
    pname = "keystone-hello-native-host";
    version = "0.1.0";
    src = ./host;

    buildInputs = [
      keystonePkgs.keystone-sdk
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
  pname = "hello-native";
  version = "0.1.0";
  inherit encApp hostApp runtime;
}
