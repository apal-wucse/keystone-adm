{
  mkShell,
  applyPatches,
  keystone-sm,
  cbmc,
  gnumake,
  ipaexfont,
  rocq-core,
  tlaplus,
  typst,
}:
mkShell {
  name = "keystone-verif-devshell";
  nativeBuildInputs = [
    cbmc
    gnumake
    rocq-core
    tlaplus
    typst
  ];

  # OpenSBI headers the SM is built with, for the CBMC harness of pmp.c
  OPENSBI_INCLUDE = "${
    applyPatches {
      name = "opensbi-${keystone-sm.opensbiVersion}-keystone-source";
      inherit (keystone-sm) src patches postPatch;
    }
  }/include";

  # Japanese fonts (IPAex Mincho / Gothic) used by verif/docs/model.typ
  VERIF_FONTS_DIR = "${ipaexfont}/share/fonts/opentype";
}
