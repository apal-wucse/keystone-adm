{
  mkShell,
  bootrom,
}:
mkShell {
  name = "keystone-bootrom-devshell";
  inputsFrom = [ bootrom ];
}
