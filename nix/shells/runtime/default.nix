{
  mkShell,
  keystonePkgs,
}:
mkShell {
  name = "eyrie-devshell";
  inputsFrom = [ keystonePkgs.runtime ];
  KEYSTONE_SDK_DIR = "${keystonePkgs.keystone-sdk}";
}
