#!/bin/sh
# QEMU checks of the fixes of MODEL.typ Section 8 (Section 10.5).
#
# usage: run-tests.sh [BIOS]
#   Boots the image of run-virt on a fresh overlay (with the SM firmware
#   BIOS instead of the one of run-virt, if given), copies admfail.ke and
#   admpeek.ke of keystoneApps into it, and runs them:
#     admfail  #1: the host breaks the shared ADM; the SM destroys the
#              enclave on resume, and the destroy by the SDK succeeds
#              "eapp checks" is 0 if a sealing key is returned and an
#              identifier over the limit is refused (#15), and a region
#              keeps its place and data when an earlier one is removed (#16)
#     iotest-adm, iotest-adm-nocpy
#              file I/O delegated through the ADM: the SM validates the
#              ADM on every resume from a writable share
#     admpeek  #7: after CPU hotplug, the host on the restarted hart has
#              no access to the ADM of an existing enclave
# Environment: RUN_VIRT (run-qemu-virt to use; default: nix build .#run-virt),
#   APPS (keystoneApps; default: nix build .#keystoneApps).
# run-virt forwards host port 2222 to the guest's ssh, which must be free.
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
RUN_VIRT=${RUN_VIRT:-$(nix build "$root#run-virt" --no-link --print-out-paths)/bin/run-qemu-virt}
APPS=${APPS:-$(nix build "$root#keystoneApps" --no-link --print-out-paths)}
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
"$sshpass" -p nixos scp -P 2222 $opts "$APPS/bin/admfail.ke" "$APPS/bin/admpeek.ke" root@127.0.0.1:/root/

# admpeek first: with an SM without the fix of #1, admfail leaks the
# enclave in the SM while the driver frees its memory.
ssh_vm 'cd /root
for c in 1 3; do
    printf "admpeek cpu=%s: " $c
    ADMPEEK_CPU=$c ./admpeek.ke 2>&1 | grep RESULT
done
for c in 0 1; do
    printf "admfail corrupt=%s: " $c
    ADMFAIL_CORRUPT=$c ./admfail.ke > out 2>&1
    if grep -q "DESTROY_ENCLAVE failed" out; then echo "destroy FAILED"
    else grep "run returned" out; fi
    grep "eapp checks" out || true
done
printf "hello-adm: "; hello-adm.ke 2>&1 | tail -1
for t in iotest-adm iotest-adm-nocpy; do
    printf "%s: " $t; $t.ke 1 2>&1 | grep -a "^Step" | tr "\n" " "; echo
done
poweroff' || true
wait $vm 2>/dev/null || true
grep -a "\[SM\] adm validation" "$work/serial.log" || true
