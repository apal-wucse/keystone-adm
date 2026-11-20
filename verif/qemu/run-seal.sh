#!/bin/sh
# QEMU check of MODEL.typ Section 8 #15 (Section 10.5).
#
# usage: run-seal.sh [BIOS]
#   Builds admfail with a runtime that asks for a sealing key at an address
#   outside the EPM (seal-outside.patch: the first bytes of the UTM), boots
#   the image of run-virt (with the SM firmware BIOS, if given) and runs it.
#   "eapp checks = 32": the SM refuses the request (expected).
#   "eapp checks = 4": the SM wrote the key outside the EPM, and accepts an
#   identifier over the limit (an SM with unfix-15.patch on sm/).
# Environment: RUN_VIRT (as run-tests.sh), APPS (the patched apps).
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
RUN_VIRT=${RUN_VIRT:-$(nix build "$root#run-virt" --no-link --print-out-paths)/bin/run-qemu-virt}
APPS=${APPS:-$(nix build --impure --no-link --print-out-paths --expr "
  let f = builtins.getFlake \"git+file://$root\";
  in (import f.inputs.nixpkgs {
    system = builtins.currentSystem;
    crossSystem.config = \"riscv64-unknown-linux-gnu\";
    overlays = [
      (import $root/nix/pkgs/riscv-overlay.nix)
      (final: prev: {
        keystonePkgs = prev.keystonePkgs // {
          runtime = prev.keystonePkgs.runtime.overrideAttrs (o: {
            patches = [ $root/verif/qemu/seal-outside.patch ];
            patchFlags = [ \"-p2\" ];
          });
        };
      })
    ];
  }).keystoneApps")}
sshpass=$(nix build nixpkgs#sshpass --no-link --print-out-paths)/bin/sshpass
work=$(mktemp -d)
opts="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
ssh_vm() { "$sshpass" -p nixos ssh -p 2222 $opts -o ConnectTimeout=10 root@127.0.0.1 "$@"; }

WORKDIR=$work "$RUN_VIRT" ${1:+-bios "$1"} < /dev/null > "$work/serial.log" 2>&1 &
vm=$!
trap 'kill $vm 2>/dev/null; rm -rf "$work"' EXIT

i=0
until ssh_vm true 2>/dev/null; do
    i=$((i + 1))
    [ $i -lt 120 ] || { echo "the VM did not come up"; exit 1; }
    sleep 5
done
"$sshpass" -p nixos scp -P 2222 $opts "$APPS/bin/admfail.ke" root@127.0.0.1:/root/
ssh_vm 'cd /root; ADMFAIL_CORRUPT=0 ./admfail.ke 2>&1 | grep "run returned\|eapp checks"; poweroff' || true
wait $vm 2>/dev/null || true
