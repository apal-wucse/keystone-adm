{
  mkShell,
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

  # Japanese serif font (IPAex Mincho) used by verif/docs/theme
  VERIF_FONTS_DIR = "${ipaexfont}/share/fonts/opentype";
}
