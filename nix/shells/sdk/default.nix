{
  stdenv,
  mkShell,
  keystonePkgs,
}:
mkShell {
  name = "keystone-sdk-devshell";
  inputsFrom = [ keystonePkgs.keystone-sdk ];

  KEYSTONE_SDK_DIR = "${keystonePkgs.keystone-sdk}";
  CROSS_COMPILE = "${stdenv.cc.targetPrefix}";
}
