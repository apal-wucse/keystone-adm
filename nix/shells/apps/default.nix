{
  mkShell,
  keystonePkgs,
  cmake,
  clang-tools,
}:
mkShell {
  name = "keystone-apps-devshell";
  buildInputs = [
    keystonePkgs.keystone-sdk
  ];
  nativeBuildInputs = [
    cmake
    clang-tools
  ];
}
