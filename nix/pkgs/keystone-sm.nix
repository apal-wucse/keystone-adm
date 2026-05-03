{
  lib,
  stdenv,
  fetchFromGitHub,
  python3,
  withPlatform ? "generic",
  withKeystonePlatform ? "generic",
  withPayload ? null,
  withFDT ? null,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "keystone-sm";
  version = "1.0.0-adm-${finalAttrs.keystonePlat}";

  opensbiVersion = "1.1"; # Keystone SM isn't conpatible with v1.2 or later.

  opensbiPlat = withPlatform;
  keystonePlat = withKeystonePlatform;

  src = fetchFromGitHub {
    owner = "riscv-software-src";
    repo = "opensbi";
    tag = "v${finalAttrs.opensbiVersion}";
    hash = "sha256-k6f4/lWY/f7qqk0AFY4tdEi4cDilSv/jngaJYhKFlnY=";
  };

  smSrc = lib.fileset.toSource {
    root = ../../sm;
    fileset = lib.fileset.unions [
      ../../sm/plat
      ../../sm/src
    ];
  };

  patches = [
    ../patches/opensbi/opensbi-change-basename.patch
    ../patches/opensbi/opensbi-firmware-secure-boot.patch
  ];

  postPatch = ''
    patchShebangs ./scripts
  '';

  nativeBuildInputs = [
    python3
  ];

  installFlags = [
    "I=$(out)"
  ];

  makeFlags = [
    "PLATFORM=${finalAttrs.opensbiPlat}"
    "KEYSTONE_PLATFORM=${finalAttrs.keystonePlat}"
    "KEYSTONE_SM=${finalAttrs.smSrc}"
    "PLATFORM_DIR=${finalAttrs.smSrc}/plat/"
  ]
  ++ lib.optionals (withPayload != null) [
    "FW_PAYLOAD_PATH=${withPayload}"
  ]
  ++ lib.optionals (withFDT != null) [
    "FW_FDT_PATH=${withFDT}"
  ];

  enableParallelBuilding = true;

  dontStrip = true;
  dontPatchELF = true;

  meta = {
    description = "RISC-V Keystone Enclave Security Monitor";
    homepage = "https://github.com/keystone-enclave/keystone";
    license = lib.licenses.bsd2;
    platforms = [
      "riscv64-linux"
      "riscv32-linux"
    ];
  };
})
