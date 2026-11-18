{
  mkShell,
  applyPatches,
  keystone-sm,
  asciidoctor-with-extensions,
  cbmc,
  gnumake,
  ipaexfont,
  rocq-core,
  tlaplus,
}:
mkShell {
  name = "keystone-verif-devshell";
  nativeBuildInputs = [
    asciidoctor-with-extensions
    cbmc
    gnumake
    rocq-core
    tlaplus
  ];

  # OpenSBI headers the SM is built with, for the CBMC harness of pmp.c
  OPENSBI_INCLUDE = "${
    applyPatches {
      name = "opensbi-${keystone-sm.opensbiVersion}-keystone-source";
      inherit (keystone-sm) src patches postPatch;
    }
  }/include";

  # Japanese serif font (IPAex Mincho) used by verif/docs/theme
  VERIF_FONTS_DIR = "${ipaexfont}/share/fonts/opentype";
}
