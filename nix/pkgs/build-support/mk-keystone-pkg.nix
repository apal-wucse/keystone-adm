{
  stdenv,
  makeself,
  lib,
}:
{
  pname,
  version,
  encApp,
  hostApp,
  runtime,
  keystonePname ? pname,
  encAppBin ? "${keystonePname}",
  hostAppBin ? "${keystonePname}-runner",
  eyrieRtBin ? "eyrie-rt",
  rtLoaderBin ? "loader.bin",
  launchCmd ? [
    "./${hostAppBin}"
    "${encAppBin}"
    "eyrie-rt"
    "loader.bin"
  ],
  ...
}@args:
stdenv.mkDerivation (
  finalAttrs:
  {
    inherit pname version;
    nativeBuildInputs = [ makeself ] ++ (args.nativeBuildInputs or [ ]);

    dontUnpack = true;
    dontBuild = true;
    dontFixup = true;

    installPhase = ''
      runHook preInstall

      mkdir -p $out/bin
      mkdir -p $TMPDIR/${keystonePname}
      cp ${encApp}/bin/${encAppBin} ${hostApp}/bin/${hostAppBin} $TMPDIR/${keystonePname}/
      cp ${runtime}/bin/${eyrieRtBin} ${runtime}/bin/${rtLoaderBin} $TMPDIR/${keystonePname}/
      makeself --noprogress \
        $TMPDIR/${keystonePname} \
        $out/bin/${keystonePname}.ke \
        "Keystone Enclave Package" \
        ${lib.escapeShellArgs launchCmd}

      runHook postInstall
    '';
  }
  // (removeAttrs args [
    "pname"
    "version"
    "keystonePname"
    "encApp"
    "hostApp"
    "runtime"
    "label"
    "nativeBuildInputs"
  ])
)
