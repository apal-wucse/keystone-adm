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

  runtimeAdm = keystonePkgs.runtime.override { inherit withPlatform; };

  encApp = stdenv.mkDerivation (finalAttrs: {
    pname = "keystone-hello-eapp";
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
    pname = "keystone-hello-host";
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

  hostAppAdm = stdenv.mkDerivation (finalAttrs: {
    pname = "keystone-hello-host-adm";
    version = "0.1.0";
    src = ./host-adm;

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

  hello = mkKeystonePackage {
    pname = "hello";
    version = "0.1.0";
    inherit encApp hostApp runtime;

    passthru = {
      adm = mkKeystonePackage {
        pname = "hello-adm";
        version = "0.1.0";
        inherit encApp;
        hostApp = hostAppAdm;
        runtime = runtimeAdm;
        encAppBin = "hello";
      };
    };
  };

in
hello
