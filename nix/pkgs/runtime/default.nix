{
  lib,
  stdenv,
  cmake,
  keystonePkgs,
  withPlatform ? "generic",
}:
let
  mkRuntime =
    {
      withFreeMem ? true,
      withPaging ? false,
      withPageEncrypt ? false,
      withPageHash ? false,
      withLinuxSyscall ? true,
      withIoSyscall ? true,
      withNetSyscall ? true,
      withEdgeProtection ? true,
      withGlibc ? true,
      withStrace ? false,
      debug ? false,
    }:
    stdenv.mkDerivation (
      finalAttrs:
      {
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
        src = ../../../runtime;

        nativeBuildInputs = [
          cmake
        ];

        hardeningDisable = [ "all" ];

        NIX_CFLAGS_COMPILE = "";
        NIX_LDFLAGS = "";

        dontFixup = true;
        dontPatchElf = true;
        dontStrip = true;

        cmakeFlags = [
          "-DCMAKE_INSTALL_PREFIX=${placeholder "out"}"
          "-DCROSS_COMPILE=${stdenv.cc.targetPrefix}"
          "-DKEYSTONE_SDK_DIR=${keystonePkgs.keystone-sdk}"
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
      }
      // {
        passthru = {
          weakSyscall =
            mkRuntime {
              withEdgeProtection = false;
            }
            // {
              passthru = {
                debug = mkRuntime {
                  withEdgeProtection = false;
                  withStrace = true;
                  debug = true;
                };
              };
            };
          nolibc =
            mkRuntime {
              withLinuxSyscall = false;
              withIoSyscall = false;
              withNetSyscall = false;
              withGlibc = false;
              withEdgeProtection = false;
            }
            // {
              passthru = {
                debug = mkRuntime {
                  withLinuxSyscall = false;
                  withIoSyscall = false;
                  withNetSyscall = false;
                  withGlibc = false;
                  withStrace = true;
                  debug = true;
                };
              };
            };

          full = mkRuntime {
            withPaging = true;
            withPageEncrypt = true;
            withPageHash = true;
            withStrace = true;
            debug = true;
          };

          debug = mkRuntime {
            withStrace = true;
            debug = true;
          };
        };
      }
    );
  runtime = mkRuntime { };
in
runtime
