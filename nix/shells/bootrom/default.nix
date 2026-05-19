{
  mkShell,
  keystonePkgs,
}:
mkShell {
  name = "keystone-bootrom-devshell";
  inputsFrom = [ keystonePkgs.bootrom ];
}
