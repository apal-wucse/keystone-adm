{
  mkShell,
  linuxPackages,
}:
mkShell {
  name = "keystone-driver-devshell";
  inputsFrom = [ linuxPackages.keystone-driver ];
}
