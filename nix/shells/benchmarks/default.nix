{
  mkShell,
  keystonePkgs,
  cmake,
  clang-tools,
}:
mkShell {
  name = "keystone-benchmarks-devshell";
  buildInputs = [
    keystonePkgs.keystone-sdk
  ];
  nativeBuildInputs = [
    cmake
    clang-tools
  ];
}
