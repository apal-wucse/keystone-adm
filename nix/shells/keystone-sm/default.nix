{
  mkShell,
  applyPatches,
  buildPackages,
  keystonePkgs,
}:
let
  sm = keystonePkgs.keystone-sm;

  # The OpenSBI tree the package actually builds from (patches applied), so
  # that a build in this shell links as well.
  opensbiSrc = applyPatches {
    name = "opensbi-${sm.opensbiVersion}-keystone-source";
    inherit (sm) src patches postPatch;
  };

  # Build the SM from the working tree under bear and write
  # compile_commands.json next to it (sm/).  Extra arguments go to make.
  sm-compile-commands = buildPackages.writeShellScriptBin "sm-compile-commands" ''
    set -eu
    cd "$KEYSTONE_SM"
    exec ${buildPackages.bear}/bin/bear -- \
      make -C "$OPENSBI_SRC" O="$KEYSTONE_SM/build" \
        PLATFORM=${sm.opensbiPlat} KEYSTONE_PLATFORM=${sm.keystonePlat} \
        KEYSTONE_SM="$KEYSTONE_SM" PLATFORM_DIR="$PLATFORM_DIR" \
        CROSS_COMPILE=${sm.stdenv.cc.targetPrefix} \
        -j"$(nproc)" "$@"
  '';
in
mkShell {
  name = "keystone-sm-devshell";
  inputsFrom = [ sm ];
  nativeBuildInputs = [
    buildPackages.bear
    sm-compile-commands
  ];

  OPENSBI_SRC = "${opensbiSrc}";

  # Point the SM sources at the working tree, not at the store snapshot, so
  # that edits are picked up and compile_commands.json matches the files
  # being edited.  Falls back to the snapshot outside a git checkout.
  shellHook = ''
    if root=$(git rev-parse --show-toplevel 2>/dev/null) && [ -d "$root/sm/src" ]; then
      export KEYSTONE_SM="$root/sm"
    else
      export KEYSTONE_SM="${sm.smSrc}"
    fi
    export PLATFORM_DIR="$KEYSTONE_SM/plat"
  '';
}
