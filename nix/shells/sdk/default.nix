{
  stdenv,
  mkShell,
  keystone-sdk,
}:
mkShell {
  name = "keystone-sdk-devshell";
  inputsFrom = [ keystone-sdk ];

  KEYSTONE_SDK_DIR = "${keystone-sdk}";
  CROSS_COMPILE = "${stdenv.cc.targetPrefix}";
}
