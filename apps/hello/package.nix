{
  stdenv,
  cmake,
  glibc,
  makeself,
  keystonePkgs,
  withPlatform ? "generic",
}:
let
  runtime =
    (keystonePkgs.runtime.override {
      inherit withPlatform;
    }).weakSyscall;

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
in
stdenv.mkDerivation (finalAttrs: {
  pname = "keystone-hello";
  version = "0.1.0";

  nativeBuildInputs = [
    makeself
  ];

  dontUnpack = true;
  dontBuild = true;
  dontFixup = true;

  installPhase = ''
    mkdir -p $out/bin
    mkdir -p $TMPDIR/pkg-files
    cp ${encApp}/bin/hello ${hostApp}/bin/hello-runner $TMPDIR/pkg-files/
    cp ${runtime}/bin/eyrie-rt ${runtime}/bin/loader.bin $TMPDIR/pkg-files/
    makeself --noprogress \
      $TMPDIR/pkg-files \
      $out/bin/hello.ke \
      "Keystone Enclave Package" \
      "./hello-runner" "hello" "eyrie-rt" "loader.bin"
  '';
})
