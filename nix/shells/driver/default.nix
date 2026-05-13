{
  mkShell,
  keystone-driver,
}:
mkShell {
  name = "keystone-driver-devshell";
  inputsFrom = [ keystone-driver ];
}
