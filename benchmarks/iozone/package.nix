{
  lib,
  mkKeystonePackage,
  keystonePkgs,
  cmake,
  symlinkJoin,
  pkgsMusl,
  withPlatform ? "generic",
}:
let
  runtime = keystonePkgs.runtime.override {
    inherit withPlatform;
    withEdgeProtection = false;
  };

  runtimeAdm = keystonePkgs.runtime.override {
    inherit withPlatform;
  };

  iozone = pkgsMusl.stdenv.mkDerivation (finalAttrs: {
    pname = "iozone";
    version = "0.1.0";

    src = ./src;

    herdeningDisable = [ "all" ];

    buildInputs = [
      keystonePkgs.keystone-sdk
    ];

    makeFlags = [
      "CFLAGS=-O0${lib.optionalString (withPlatform == "generic") " -DRISCV_RTC_1M"}"
    ];

    buildPhase = ''
      runHook preBuild
      make ${lib.escapeShellArgs finalAttrs.makeFlags} all
      runHook postBuild
    '';

    installPhase = ''
      runHook preInstall
      mkdir -p $out/bin
      install -m755 iozone-rw $out/bin/
      install -m755 iozone-rw-1m $out/bin/
      install -m755 iozone-rw-adm $out/bin/
      install -m755 iozone-rw-adm-1m $out/bin/
      runHook postInstall
    '';
  });

  hostApp = pkgsMusl.stdenv.mkDerivation (finalAttrs: {
    pname = "iozone-host";
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

  iozone-baseline = mkKeystonePackage {
    pname = "iozone-baseline";
    version = "0.1.0";
    inherit runtime hostApp;
    encApp = iozone;
    encAppBin = "iozone-rw-1m";
    hostAppBin = "iozone-ecall-runner";
  };

  iozone-adm = mkKeystonePackage {
    pname = "iozone-adm";
    version = "0.1.0";
    inherit hostApp;
    runtime = runtimeAdm;
    encApp = iozone;
    encAppBin = "iozone-rw-adm";
    hostAppBin = "iozone-adm-runner";
  };

  iozone-adm-small = mkKeystonePackage {
    pname = "iozone-adm-small";
    version = "0.1.0";
    inherit hostApp;
    runtime = runtimeAdm;
    encApp = iozone;
    encAppBin = "iozone-rw-adm-1m";
    hostAppBin = "iozone-adm-runner";
  };
in
symlinkJoin {
  pname = "iozone-benchmarks";
  version = "0.1.0";

  paths = [
    iozone
    iozone-baseline
    iozone-adm
    iozone-adm-small
  ];
}
