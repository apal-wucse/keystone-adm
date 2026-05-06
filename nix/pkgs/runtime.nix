{
  lib,
  stdenv,
  cmake,
  keystone-sdk,
  withPlatform ? "generic",
  withFreeMem ? false,
  withPaging ? false,
  withPageEncrypt ? false,
  withPageHash ? false,
  withLinuxSyscall ? false,
  withIoSyscall ? false,
  withNetSyscall ? false,
  withEdgeProtection ? false,
  withGlibc ? false,
  withStrace ? false,
  debug ? false,
}:
stdenv.mkDerivation (finalAttrs: {
  verWithOptions = [
    "1.0.0-adm"
  ]
  ++ lib.optional withFreeMem "fm"
  ++ lib.optional withPaging "pg"
  ++ lib.optional withPageEncrypt "enc"
  ++ lib.optional withPageHash "hash"
  ++ lib.optional withLinuxSyscall "lsys"
  ++ lib.optional withIoSyscall "io"
  ++ lib.optional withNetSyscall "net"
  ++ lib.optional withEdgeProtection "ep"
  ++ lib.optional withGlibc "libc"
  ++ lib.optional withStrace "st"
  ++ lib.optional debug "debug";

  pname = "eyrie";
  version = lib.concatStringsSep "_" finalAttrs.verWithOptions;
  src = ../../runtime;

  nativeBuildInputs = [
    cmake
  ];

  cmakeFlags = [
    "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}"
    "-DCROSS_COMPILE=${stdenv.cc.targetPrefix}"
    "-DKEYSTONE_SDK_DIR=${keystone-sdk}"
  ]
  ++ lib.optional withFreeMem "-DFREEMEM=ON"
  ++ lib.optional withPaging "-DPAGING=ON"
  ++ lib.optional withPageEncrypt "-DPAGE_CRYPTO=ON"
  ++ lib.optional withPageHash "-DPAGE_HASH=ON"
  ++ lib.optional withLinuxSyscall "-DLINUX_SYSCALL=ON"
  ++ lib.optional withIoSyscall "-DIO_SYSCALL=ON"
  ++ lib.optional withNetSyscall "-DNET_SYSCALL=ON"
  ++ lib.optional withEdgeProtection "-DEDGE_PROTECTION=ON"
  ++ lib.optional withGlibc "-DENV_SETUP=ON"
  ++ lib.optional withStrace "-DINTERNAL_STRACE=ON"
  ++ lib.optional debug "-DDEBUG=ON"
  ++ lib.optional (withPlatform == "unmatched") "-DPLATFORM_HIFIVE_UNMATCHED=ON";
})
