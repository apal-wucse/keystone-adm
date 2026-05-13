{
  mkShell,
  keystone-sm,
}:
mkShell {
  name = "keystone-sm-devshell";
  inputsFrom = [ keystone-sm ];

  OPENSBI_SRC = "${keystone-sm.src}";
  KEYSTONE_SM = "${keystone-sm.smSrc}";
  PLATFORM_DIR = "${keystone-sm.smSrc}/plat";
}
