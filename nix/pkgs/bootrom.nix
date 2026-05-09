{
  lib,
  stdenv,
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "keystone-bootrom";
  version = "1.0.0";

  src = ../../bootloader/bootrom;

  crypto = lib.fileset.toSource {
    root = ../../crypto;
    fileset = lib.fileset.unions [
      ../../crypto/sha3
      ../../crypto/ed25519
    ];
  };

  postUnpack = ''
    chmod -R u+w $sourceRoot
  '';

  prePatch = ''
    cp -r ${finalAttrs.crypto}/ed25519 .
    cp -r ${finalAttrs.crypto}/sha3 .
  '';

  preBuild = ''
    mkdir -p $out/bin
  '';

  hardeningDisable = [ "all" ];

  makeFlags = [
    "O=${placeholder "out"}/bin"
  ];

  dontInstall = true;
  dontStrip = true;
  dontPatchELF = true;
  dontFixup = true;
})
