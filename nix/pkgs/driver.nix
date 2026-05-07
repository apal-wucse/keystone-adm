{
  stdenv,
  kernel,
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "keystone-driver-adm";
  version = "1.0.0";

  src = ../../driver;
  hardeningDisable = [
    "pic"
    "format"
  ];
  nativeBuildInputs = kernel.moduleBuildDependencies;

  makeFlags = [
    "KERNELRELEASE=${kernel.modDirVersion}"
    "LINUXSRC=${kernel.dev}/lib/modules/${kernel.modDirVersion}/build"
    "INSTALL_MOD_PATH=${placeholder "out"}"
  ];
})
