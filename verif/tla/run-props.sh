#!/bin/sh
# Check every property of ADMImpl.tla in its own TLC run and report, for
# each one, whether it holds or is violated.  The counterexample of a
# violated property is left in OUTDIR/<property>.log.
#
# usage: run-props.sh CONFIG OUTDIR
#   CONFIG  a configuration holding the constants only
#   OUTDIR  directory for the generated configurations and the logs
# Environment: TLC (default tlc), TLC_FLAGS.
# Exit status: non-zero only if a TLC run neither found an error nor
# completed (a tool or model error), not when a property is violated.

set -u
cfg=$1
out=$2
TLC=${TLC:-tlc}
TLC_FLAGS=${TLC_FLAGS:--deadlock -workers auto}

INVS="A1_InitialProtection A2_ExclusiveWhileRunning A3_OnlyExplicitGrant
A6_IsolatedWhenNotShared A8_InvoluntaryStopSafe E1_NoUnintendedDisclosure
B1_ValidatedBeforeRun B2_NoAccessDuringValidation B3_FailClosed
B3_NeverRunsAgain B4_FreshMetadata E2_ContentsPreserved D2d_SingleRunner
D3_SingleDestroyer"
PROPS="A4_RevokedBeforeEnter A9_NoSecretAtRelease B5_EnterOnlyViaValidation"

here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$out"
fail=
for p in $INVS $PROPS; do
    kind=INVARIANT
    case " $PROPS " in *" $p "*) kind=PROPERTY ;; esac
    { cat "$cfg"; printf '%s %s\n' "$kind" "$p"; } > "$out/$p.cfg"
    (cd "$here" && $TLC $TLC_FLAGS -metadir "$out/meta-$p" \
        -config "$out/$p.cfg" ADMImpl.tla) > "$out/$p.log" 2>&1
    if grep -q "is violated" "$out/$p.log"; then
        n=$(grep -c '^State [0-9]*:' "$out/$p.log")
        printf 'VIOLATED  %-28s (counterexample: %s states)\n' "$p" "$n"
    elif grep -q "No error has been found" "$out/$p.log"; then
        printf 'holds     %s\n' "$p"
    else
        printf 'ERROR     %s (see %s)\n' "$p" "$out/$p.log"
        fail=1
    fi
done
test -z "$fail"
