{
  mkShell,
  keystonePkgs,
}:
mkShell {
  name = "keystone-sm-devshell";
  inputsFrom = [ keystonePkgs.keystone-sm ];

  OPENSBI_SRC = "${keystonePkgs.keystone-sm.src}";
  KEYSTONE_SM = "${keystonePkgs.keystone-sm.smSrc}";
  PLATFORM_DIR = "${keystonePkgs.keystone-sm.smSrc}/plat";
}
