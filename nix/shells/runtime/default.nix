{
  mkShell,
  runtime,
  keystone-sdk,
}:
mkShell {
  name = "eyrie-devshell";
  inputsFrom = [ runtime ];
  KEYSTONE_SDK_DIR = "${keystone-sdk}";
}
