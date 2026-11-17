{
  mkShell,
  linuxPackages_6_12,
}:
mkShell {
  name = "keystone-driver-devshell";
  inputsFrom = [ linuxPackages_6_12.keystone-driver ];
}
