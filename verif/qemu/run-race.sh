#!/bin/sh
# QEMU check of the stop/resume race of MODEL.typ Section 8 #13 (Section 10.5).
#
# usage: run-race.sh [BIOS]
#   Boots the image of run-virt on a fresh overlay (with the SM firmware
#   BIOS instead of the one of run-virt, if given) and runs admrace.ke.
#   CONTROL: the CPUs that can read the ADM during a stop with the share
#   (0xf expected) and during a stop without a share (0x0 expected).
#   RESULT exposed=N: in N of the checked rounds a host CPU could read the
#   ADM while it was not shared. A VM that stops answering is reported too:
#   with an SM without the fix (unfix-13.patch on sm/), the CPU running the
#   enclave locks up within seconds.
# Environment: RUN_VIRT, APPS (as run-tests.sh), ADMRACE_TRIALS (20000),
#   ADMRACE_SPIN (1000), LIMIT (seconds for admrace, default 1800),
#   HOST_CPUS (list for taskset; fewer host CPUs than guest CPUs makes
#   the guest CPUs stall at arbitrary points).
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
RUN_VIRT=${RUN_VIRT:-$(nix build "$root#run-virt" --no-link --print-out-paths)/bin/run-qemu-virt}
APPS=${APPS:-$(nix build "$root#keystoneApps" --no-link --print-out-paths)}
sshpass=$(nix build nixpkgs#sshpass --no-link --print-out-paths)/bin/sshpass
work=$(mktemp -d)
opts="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
ssh_vm() { "$sshpass" -p nixos ssh -p 2222 $opts -o ConnectTimeout=10 root@127.0.0.1 "$@"; }

WORKDIR=$work ${HOST_CPUS:+taskset -c "$HOST_CPUS"} "$RUN_VIRT" ${1:+-bios "$1"} \
    < /dev/null > "$work/serial.log" 2>&1 &
vm=$!
trap 'kill $vm 2>/dev/null; rm -rf "$work"' EXIT

i=0
until ssh_vm true 2>/dev/null; do
    i=$((i + 1))
    [ $i -lt 240 ] || { echo "the VM did not come up"; exit 1; }
    sleep 5
done
"$sshpass" -p nixos scp -P 2222 $opts "$APPS/bin/admrace.ke" root@127.0.0.1:/root/

if ! timeout "${LIMIT:-1800}" "$sshpass" -p nixos ssh -p 2222 $opts root@127.0.0.1 \
    "cd /root; ADMRACE_TRIALS=${ADMRACE_TRIALS:-20000} ADMRACE_SPIN=${ADMRACE_SPIN:-1000} \
     ./admrace.ke 2>&1 | tee /dev/console | grep -a 'CONTROL\|RESULT' | tail -20; exit 0"; then
    echo "admrace did not finish"
fi
if ssh_vm true 2>/dev/null; then
    ssh_vm poweroff || true
    wait $vm 2>/dev/null || true
else
    echo "the VM does not answer"
fi
grep -a "\[host\]\|CONTROL\|RESULT\|adm validation\|sbi_trap_error\|Oops\|panic\|LOCKUP" "$work/serial.log" |
    tail -25 || true
