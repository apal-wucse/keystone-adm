{
  stdenv,
  cmake,
  glibc,
  mkKeystonePackage,
  keystonePkgs,
  symlinkJoin,
  withPlatform ? "generic",
}:
let
  runtime = keystonePkgs.runtime.override {
    inherit withPlatform;
    withEdgeProtection = false;
  };

  encApp = stdenv.mkDerivation (finalAttrs: {
    pname = "cswitch-eapp";
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

  encAppAdm = encApp.overrideAttrs (
    final: prev: {
      pname = "cswitch-eapp-adm";
      cmakeFlags = prev.cmakeFlags ++ [ "-DADM_BUILD=ON" ];
    }
  );

  encAppAdmRw = encApp.overrideAttrs (
    final: prev: {
      pname = "cswitch-eapp-adm-rw";
      cmakeFlags = prev.cmakeFlags ++ [ "-DADM_RW_BUILD=ON" ];
    }
  );

  hostApp = stdenv.mkDerivation (finalAttrs: {
    pname = "cswitch-host";
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

  hostAppAdm = hostApp.overrideAttrs (
    final: prev: {
      pname = "cswitch-host-adm";
      cmakeFlags = prev.cmakeFlags ++ [ "-DADM_BUILD=ON" ];
    }
  );

  cswitch = mkKeystonePackage {
    pname = "cswitch-baseline";
    version = "0.1.0";
    inherit encApp hostApp runtime;
    hostAppBin = "cswitch-runner";
    encAppBin = "cswitch";
  };

  cswitch-adm = mkKeystonePackage {
    pname = "cswitch-adm";
    version = "0.1.0";
    encApp = encAppAdm;
    hostApp = hostAppAdm;
    inherit runtime;
    hostAppBin = "cswitch-runner";
    encAppBin = "cswitch";
  };

  cswitch-adm-rw = mkKeystonePackage {
    pname = "cswitch-adm-rw";
    version = "0.1.0";
    encApp = encAppAdmRw;
    hostApp = hostAppAdm;
    inherit runtime;
    hostAppBin = "cswitch-runner";
    encAppBin = "cswitch";
  };
in
symlinkJoin {
  pname = "cswitch-benchmarks";
  version = "0.1.0";

  paths = [
    cswitch
    cswitch-adm
    cswitch-adm-rw
  ];
}
