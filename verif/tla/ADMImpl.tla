------------------------------- MODULE ADMImpl -------------------------------
(***************************************************************************)
(* Implementation model of ADM / Protected Sharing (MODEL.adoc Section 8). *)
(*                                                                         *)
(* This module is independent of ADM.tla: the design model is left as it  *)
(* is.  It follows sm/src/enclave.c, adm.c, ipi.c, pmp.c and sm.c of this  *)
(* repository and OpenSBI 1.1, with the steps split where the code drops  *)
(* the enclave lock or crosses harts:                                      *)
(*                                                                         *)
(*   run_enclave      RunBegin (lock) -> RunValidate (+ enter)             *)
(*   resume_enclave   ResumeBegin (lock) -> ResumeRevoke -> ResumeValidate *)
(*                    (+ enter)                                            *)
(*   destroy_enclave  DestroyBegin (lock) -> DestroyZero -> DestroyUnset   *)
(*                    -> DestroyReset                                      *)
(*   stop_enclave, stop_enclave_with_share, exit_enclave                   *)
(*                    StopInvoluntary / StopShare / Exit (lock)            *)
(*                    -> StopSwitch (context_switch_to_host, unlocked)     *)
(*   HSM (OpenSBI 1.1 sbi_hsm.c, sbi_init.c)                               *)
(*     hart_start   STOPPED -> PENDING -(sm_init(false))-> PENDING_R       *)
(*                  -(prepare_next_jump)-> STARTING -(to S-mode)-> STARTED *)
(*     hart_suspend STARTED -> SUSPENDED -> STARTED (retentive) or         *)
(*                  -> RESUMING -(resume_finish)-> STARTING (non-ret.)     *)
(*     hart_stop    STARTED -> STOPPED                                     *)
(*                                                                         *)
(* PMP: entry[e][h] is the ADM entry of e on hart h: a permission, or      *)
(* "UNSET" when the entry is not programmed on that hart.  An unset entry  *)
(* falls through to the lowest-priority OS region, which grants            *)
(* everything to the host context and nothing to an enclave context        *)
(* (osm_pmp_set).  Eff(e, h) is the resulting S/U-mode permission; a hart  *)
(* that is not running S/U-mode code (not STARTED, or in the SM) has none. *)
(* pmp_set_global reaches only the harts in the interruptible mask         *)
(* (STARTED or SUSPENDED as OpenSBI sees them); its IPI round is modeled   *)
(* as one atomic step because the caller waits for completion.             *)
(*                                                                         *)
(* The constants Fix* select a corrected implementation, one difference   *)
(* of MODEL.adoc Section 8 at a time.  All FALSE is the code before any    *)
(* fix.  RestoreAt selects how #7 is fixed when FixHartRestart is TRUE.    *)
(*                                                                         *)
(* Abstractions: DEFAULT_NOPERM only; one thread; EPM and UTM are not      *)
(* modeled; create_enclave is atomic (ALLOCATED is not modeled); the ADM   *)
(* contents are one abstract value from Data, and validate_adm_regions     *)
(* accepts the contents iff they match some used metadata slot (it looks   *)
(* each region of the ADM up among the slots, Section 8 #2).  The design-  *)
(* level expectation is that they match what the enclave supplied.         *)
(***************************************************************************)
EXTENDS Naturals, FiniteSets

CONSTANTS
    Harts, EIDs, Data, Zero, Meta,
    NSlots,             \* ADM_SLOT_MAX (abstracted)
    Unused,             \* ADM_UID_UNUSED
    FixValidateFail,    \* #1  destroy the enclave when validation fails
    FixStaleMeta,       \* #5  replace all slots on stop, clear them on resume
    FixNullRW,          \* #12 check the arguments and the state before touching
                        \*     the state (RW without metadata, not RUNNING)
    FixShareType,       \* #6  reject an out-of-range share_type up front
    FixDestroyGuard,    \* #11 accept destroy only in FRESH / STOPPED
    FixHartRestart,     \* #7  re-program the enclave entries on a restarted
                        \*     hart from the records rec (where: RestoreAt)
    RestoreAt,          \* #7  "init_resend": in sm_init(false), and senders
                        \*       re-send to START_PENDING harts once STARTED
                        \*     "init": in sm_init(false) only
                        \*     "started": in an OpenSBI hook after STARTED,
                        \*       before the jump to S-mode (needs a patch)
    FixNonRetSuspend,   \* #7  an SM HSM device refuses non-retentive suspend
    FixStopping         \* #13 STOPPING until context_switch_to_host is done:
                        \*     no resume / destroy in between

ASSUME /\ Zero \in Data
       /\ Meta \subseteq Data \ {Zero}
       /\ NSlots \in Nat \ {0}
       /\ Unused \notin Meta
       /\ RestoreAt \in {"init_resend", "init", "started"}

VARIABLES
    state,      \* enclaves[e].state
    nthread,    \* enclaves[e].n_thread
    share,      \* enclaves[e].adm_state_info.share ("BAD": out of range)
    slots,      \* enclaves[e].adm_state_info.type_info
    ctx,        \* context of hart h
    op,         \* SM operation in progress on hart h
    hsm,        \* HSM state of hart h
    entry,      \* entry[e][h]: ADM PMP entry of e on hart h
    rec,        \* rec[e]: last value set on every hart (pmp_set_global /
                \* pmp_unset_global); kept by the #7 fix, unused otherwise
    mem,        \* ADM contents
    \* ghost variables (as in ADM.tla)
    secret, dirty, sentMeta, dead, lastStop, snap

vars == <<state, nthread, share, slots, ctx, op, hsm, entry, rec, mem,
          secret, dirty, sentMeta, dead, lastStop, snap>>

Slot     == 1..NSlots
Perm     == {"NONE", "R", "RW"}
EntryV   == Perm \cup {"UNSET"}
ShareV   == {"NONE", "RONLY", "RW", "BAD"}
State    == {"INVALID", "FRESH", "RUNNING", "STOPPED", "DESTROYING"}
Op       == {"none", "run", "resume_revoke", "resume_validate", "stop_switch",
             "d_zero", "d_unset", "d_reset"}
StopKind == {"none", "share_none", "share_ro", "share_rw", "invol", "exit"}

Host   == <<"HOST", "">>
Enc(e) == <<"ENC", e>>
Sm(e)  == <<"SM", e>>
Ctx    == {Host} \cup {Enc(e) : e \in EIDs} \cup {Sm(e) : e \in EIDs}

HostPerm(s) == CASE s = "RONLY" -> "R"
                 [] s = "RW"    -> "RW"
                 [] OTHER       -> "NONE"

PermRank(p) == CASE p = "NONE" -> 0 [] p = "R" -> 1 [] p = "RW" -> 2
PermLeq(p, q) == PermRank(p) <= PermRank(q)

\* Metadata supplied to stop_enclave_with_share: a list of `count`
\* entries (count <= ADM_SLOT_MAX; larger counts are rejected before
\* anything is touched).  A NULL pointer is a separate argument below.
Prov == UNION {[1..n -> Meta] : n \in 0..NSlots}
Range(p) == {p[i] : i \in DOMAIN p}

NoSlots == [i \in Slot |-> Unused]
Used(sl) == {sl[i] : i \in {j \in Slot : sl[j] # Unused}}

\* Copy of the first `count` entries (NULL: none); the rest keep their old
\* values, or are cleared (FixStaleMeta).
Overwrite(sl, p) ==
    [i \in Slot |-> IF i \in DOMAIN p THEN p[i]
                    ELSE IF FixStaleMeta THEN Unused ELSE sl[i]]

\* validate_adm_regions, abstracted (see the header).
Valid(e) == mem[e] \in Used(slots[e])

\* STARTED: running S-mode code.  STARTING: STARTED for OpenSBI but still
\* in M-mode before the jump to S-mode.  PENDING / RESUMING: START_PENDING
\* or resuming, before STARTED.
HsmState == {"STARTED", "STOPPED", "PENDING", "PENDING_R", "SUSPENDED",
             "RESUMING", "STARTING"}
Started == {h \in Harts : hsm[h] = "STARTED"}
\* sbi_hsm_hart_interruptible_mask: STARTED or SUSPENDED (STARTING is
\* STARTED as far as OpenSBI is concerned).
InMask(h) == hsm[h] \in {"STARTED", "SUSPENDED", "STARTING"}

\* S/U-mode permission of hart h on e's ADM.
Eff(e, h) ==
    IF hsm[h] # "STARTED" THEN "NONE"
    ELSE IF ctx[h] \in {Sm(x) : x \in EIDs} THEN "NONE"   \* in M-mode
    ELSE IF entry[e][h] = "UNSET"
         THEN IF ctx[h] \in {Enc(x) : x \in EIDs} THEN "NONE" ELSE "RW"
         ELSE entry[e][h]

\* pmp_set_global / pmp_unset_global: the interruptible harts only.  With
\* the "init_resend" fix, the sender also waits for every START_PENDING
\* hart to become STARTED and sends the update again.  A hart that has not
\* yet run sm_init(false) (PENDING) reads the record later; one that has
\* (PENDING_R) gets the update.  Folding the re-send into the round is
\* sound because such a hart runs no S-mode code until it is STARTED.
Reached(h) == \/ InMask(h)
              \/ FixHartRestart /\ RestoreAt = "init_resend" /\ hsm[h] = "PENDING_R"
SetG(e, v) ==
    [entry EXCEPT ![e] = [h \in Harts |-> IF Reached(h) THEN v
                                          ELSE entry[e][h]]]
RecG(e, v) == rec' = [rec EXCEPT ![e] = v]

TypeOK ==
    /\ state    \in [EIDs -> State]
    /\ nthread  \in [EIDs -> {0, 1}]
    /\ share    \in [EIDs -> ShareV]
    /\ slots    \in [EIDs -> [Slot -> Meta \cup {Unused}]]
    /\ ctx      \in [Harts -> Ctx]
    /\ op       \in [Harts -> Op]
    /\ hsm      \in [Harts -> HsmState]
    /\ rec      \in [EIDs -> EntryV]
    /\ entry    \in [EIDs -> [Harts -> EntryV]]
    /\ mem      \in [EIDs -> Data]
    /\ secret   \in [EIDs -> BOOLEAN]
    /\ dirty    \in [EIDs -> BOOLEAN]
    /\ sentMeta \in [EIDs -> SUBSET Meta]
    /\ dead     \in [EIDs -> BOOLEAN]
    /\ lastStop \in [EIDs -> StopKind]
    /\ snap     \in [EIDs -> Data]

Init ==
    /\ state    = [e \in EIDs |-> "INVALID"]
    /\ nthread  = [e \in EIDs |-> 0]
    /\ share    = [e \in EIDs |-> "NONE"]
    /\ slots    = [e \in EIDs |-> NoSlots]
    /\ ctx      = [h \in Harts |-> Host]
    /\ op       = [h \in Harts |-> "none"]
    /\ hsm      = [h \in Harts |-> "STARTED"]
    /\ entry    = [e \in EIDs |-> [h \in Harts |-> "UNSET"]]
    /\ rec      = [e \in EIDs |-> "UNSET"]
    /\ mem      = [e \in EIDs |-> Zero]
    /\ secret   = [e \in EIDs |-> FALSE]
    /\ dirty    = [e \in EIDs |-> FALSE]
    /\ sentMeta = [e \in EIDs |-> {}]
    /\ dead     = [e \in EIDs |-> FALSE]
    /\ lastStop = [e \in EIDs |-> "none"]
    /\ snap     = [e \in EIDs |-> Zero]

HostReady(h) == ctx[h] = Host /\ op[h] = "none" /\ hsm[h] = "STARTED"

\* A stop has released the lock but not yet updated the PMP (#13).
Stopping(e) == \E h \in Harts : ctx[h] = Sm(e) /\ op[h] = "stop_switch"

-----------------------------------------------------------------------------
(* create_enclave (atomic).  The initial type_info is measured; here it is *)
(* one entry m0.                                                           *)

Create(h, e, d, m0) ==
    /\ HostReady(h)
    /\ state[e] = "INVALID"
    /\ state'    = [state EXCEPT ![e] = "FRESH"]
    /\ nthread'  = [nthread EXCEPT ![e] = 0]
    /\ share'    = [share EXCEPT ![e] = "RW"]
    /\ slots'    = [slots EXCEPT ![e] = [i \in Slot |-> IF i = 1 THEN m0 ELSE Unused]]
    \* pmp_set_global(adm_region, PMP_NO_PERM); stopped harts get nothing.
    /\ entry'    = [entry EXCEPT ![e] = [x \in Harts |->
                        IF Reached(x) THEN "NONE" ELSE "UNSET"]]
    /\ RecG(e, "NONE")
    /\ mem'      = [mem EXCEPT ![e] = d]
    /\ secret'   = [secret EXCEPT ![e] = FALSE]
    /\ dirty'    = [dirty EXCEPT ![e] = TRUE]
    /\ sentMeta' = [sentMeta EXCEPT ![e] = {m0}]
    /\ dead'     = [dead EXCEPT ![e] = FALSE]
    /\ lastStop' = [lastStop EXCEPT ![e] = "none"]
    /\ UNCHANGED <<ctx, op, hsm, snap>>

-----------------------------------------------------------------------------
(* run_enclave / resume_enclave                                            *)

\* context_switch_to_enclave: the ADM entry is set to RW on this hart only.
Enter(h, e) ==
    /\ entry' = [entry EXCEPT ![e][h] = "RW"]
    /\ ctx'   = [ctx EXCEPT ![h] = Enc(e)]
    /\ op'    = [op EXCEPT ![h] = "none"]

\* Validation failure: return an error to the host (as implemented), or
\* start destroying the enclave on this hart (FixValidateFail).
Fail(h, e) ==
    /\ dead' = [dead EXCEPT ![e] = TRUE]
    /\ IF FixValidateFail
       THEN /\ state' = [state EXCEPT ![e] = "DESTROYING"]
            /\ op'    = [op EXCEPT ![h] = "d_zero"]
            /\ UNCHANGED ctx
       ELSE /\ ctx' = [ctx EXCEPT ![h] = Host]
            /\ op'  = [op EXCEPT ![h] = "none"]
            /\ UNCHANGED state

\* Successful validation: share := NONE and type_info cleared.
Accept(e) ==
    /\ share' = [share EXCEPT ![e] = "NONE"]
    /\ slots' = [slots EXCEPT ![e] = NoSlots]
    /\ dirty' = [dirty EXCEPT ![e] = FALSE]

RunBegin(h, e) ==
    /\ HostReady(h)
    /\ state[e] = "FRESH"
    /\ state'   = [state EXCEPT ![e] = "RUNNING"]
    /\ nthread' = [nthread EXCEPT ![e] = 1]
    /\ ctx'     = [ctx EXCEPT ![h] = Sm(e)]
    /\ op'      = [op EXCEPT ![h] = "run"]
    /\ UNCHANGED <<share, slots, hsm, entry, mem, secret, dirty, sentMeta,
                   dead, lastStop, snap>>

RunValidate(h, e) ==
    /\ ctx[h] = Sm(e) /\ op[h] = "run"
    /\ IF share[e] = "RW"
       THEN IF Valid(e)
            THEN /\ Accept(e) /\ Enter(h, e)
                 /\ UNCHANGED <<state, dead>>
            ELSE /\ Fail(h, e)
                 /\ UNCHANGED <<share, slots, dirty, entry>>
       ELSE \* "share state must be ADM_SHARE_RW before running"
            /\ ctx' = [ctx EXCEPT ![h] = Host]
            /\ op'  = [op EXCEPT ![h] = "none"]
            /\ UNCHANGED <<state, share, slots, dirty, entry, dead>>
    /\ UNCHANGED <<nthread, hsm, mem, secret, sentMeta, lastStop, snap>>

ResumeBegin(h, e) ==
    /\ HostReady(h)
    /\ state[e] \in {"RUNNING", "STOPPED"}
    /\ nthread[e] = 0
    /\ ~(FixStopping /\ Stopping(e))
    /\ state'   = [state EXCEPT ![e] = "RUNNING"]
    /\ nthread' = [nthread EXCEPT ![e] = 1]
    /\ ctx'     = [ctx EXCEPT ![h] = Sm(e)]
    /\ op'      = [op EXCEPT ![h] = "resume_revoke"]
    /\ UNCHANGED <<share, slots, hsm, entry, mem, secret, dirty, sentMeta,
                   dead, lastStop, snap>>

\* "restore default permission": pmp_set_global(rid, PMP_NO_PERM) when
\* shared; nothing when share is NONE ("assume another harts have no
\* permission").
ResumeRevoke(h, e) ==
    /\ ctx[h] = Sm(e) /\ op[h] = "resume_revoke"
    /\ entry' = IF share[e] \in {"RW", "RONLY"} THEN SetG(e, "NONE") ELSE entry
    /\ rec'   = IF share[e] \in {"RW", "RONLY"} THEN [rec EXCEPT ![e] = "NONE"] ELSE rec
    /\ op'    = [op EXCEPT ![h] = "resume_validate"]
    /\ UNCHANGED <<state, nthread, share, slots, ctx, hsm, mem, secret, dirty,
                   sentMeta, dead, lastStop, snap>>

ResumeValidate(h, e) ==
    /\ ctx[h] = Sm(e) /\ op[h] = "resume_validate"
    /\ CASE share[e] = "RW" ->
              IF Valid(e)
              THEN /\ Accept(e)
                   /\ Enter(h, e)
                   /\ UNCHANGED <<state, dead>>
              ELSE /\ Fail(h, e)
                   /\ UNCHANGED <<share, slots, dirty, entry>>
         [] share[e] = "RONLY" ->
              /\ share' = [share EXCEPT ![e] = "NONE"]
              /\ slots' = IF FixStaleMeta THEN [slots EXCEPT ![e] = NoSlots]
                          ELSE slots
              /\ Enter(h, e)
              /\ UNCHANGED <<state, dirty, dead>>
         [] OTHER ->
              /\ Enter(h, e)
              /\ UNCHANGED <<state, share, slots, dirty, dead>>
    /\ UNCHANGED <<nthread, hsm, mem, secret, sentMeta, lastStop, snap>>

-----------------------------------------------------------------------------
(* Stops, issued by the enclave on hart h (caller: enclave context).       *)

\* context_switch_to_host with sharing state s (DEFAULT_NOPERM).
SwitchToHost(h, e, s) ==
    /\ ctx'   = [ctx EXCEPT ![h] = Host]
    /\ entry' = CASE s = "RW"    -> SetG(e, "RW")
                  [] s = "RONLY" -> SetG(e, "R")
                  [] OTHER       -> [entry EXCEPT ![e][h] = "NONE"]
    /\ rec'   = CASE s = "RW"    -> [rec EXCEPT ![e] = "RW"]
                  [] s = "RONLY" -> [rec EXCEPT ![e] = "R"]
                  [] OTHER       -> rec

\* Ghost updates of a stop that reaches the host with sharing state s.
StopGhosts(e, s, kind) ==
    /\ secret'   = IF s \in {"RW", "RONLY"} THEN [secret EXCEPT ![e] = FALSE]
                   ELSE secret
    /\ dirty'    = IF s = "RW" THEN [dirty EXCEPT ![e] = TRUE] ELSE dirty
    /\ lastStop' = [lastStop EXCEPT ![e] = kind]
    /\ snap'     = [snap EXCEPT ![e] = mem[e]]

\* n_thread-- and STOPPED when it reaches zero (only when stoppable).
Dec(e) ==
    /\ nthread' = [nthread EXCEPT ![e] = nthread[e] - 1]
    /\ state'   = IF nthread[e] - 1 = 0 THEN [state EXCEPT ![e] = "STOPPED"]
                  ELSE state

\* The lock is dropped; context_switch_to_host follows (StopSwitch).
ToSwitch(h, e) ==
    /\ ctx' = [ctx EXCEPT ![h] = Sm(e)]
    /\ op'  = [op EXCEPT ![h] = "stop_switch"]

\* stop_enclave (timer interrupt, edge call without sharing):
\* share := NONE even when not stoppable.
StopInvoluntary(h, e) ==
    /\ ctx[h] = Enc(e)
    /\ share' = [share EXCEPT ![e] = "NONE"]
    /\ IF state[e] = "RUNNING"
       THEN /\ Dec(e)
            /\ ToSwitch(h, e)
            /\ StopGhosts(e, "NONE", "invol")
       ELSE UNCHANGED <<state, nthread, ctx, op, secret, dirty, lastStop, snap>>
    /\ UNCHANGED <<slots, hsm, entry, rec, mem, sentMeta, dead>>

\* exit_enclave: the share state is left as it is.
Exit(h, e) ==
    /\ ctx[h] = Enc(e)
    /\ state[e] = "RUNNING"
    /\ Dec(e)
    /\ ToSwitch(h, e)
    /\ StopGhosts(e, share[e], "exit")
    /\ UNCHANGED <<share, slots, hsm, entry, rec, mem, sentMeta, dead>>

\* context_switch_to_host, run after the lock is released: it reads the
\* sharing state as it is now (#13), and maps an out-of-range value to NONE.
StopSwitch(h, e) ==
    /\ ctx[h] = Sm(e) /\ op[h] = "stop_switch"
    /\ SwitchToHost(h, e, share[e])
    /\ share' = IF share[e] = "BAD" THEN [share EXCEPT ![e] = "NONE"] ELSE share
    /\ op' = [op EXCEPT ![h] = "none"]
    /\ UNCHANGED <<state, nthread, slots, hsm, mem, secret, dirty, sentMeta,
                   dead, lastStop, snap>>

Kind(s) == CASE s = "RONLY" -> "share_ro"
             [] s = "RW"    -> "share_rw"
             [] OTHER       -> "share_none"

\* stop_enclave_with_share(type_info = (isNull ? NULL : p), count = |p|,
\* share_type = s).
StopShare(h, e, s, isNull, p) ==
    LET stoppable == state[e] = "RUNNING"
        nullRW    == isNull /\ s = "RW"
        sl        == Overwrite(slots[e], IF isNull THEN <<>> ELSE p)
    IN
    /\ ctx[h] = Enc(e)
    \* With the fixes, these calls are rejected before anything changes.
    /\ ~(FixShareType /\ s = "BAD")
    /\ ~(FixNullRW /\ (nullRW \/ ~stoppable))
    /\ CASE nullRW ->
              \* n_thread / state already updated; share := NONE; return
              \* ILLEGAL_ARGUMENT to the enclave, which keeps running.
              /\ IF stoppable THEN Dec(e) ELSE UNCHANGED <<state, nthread>>
              /\ share' = [share EXCEPT ![e] = "NONE"]
              /\ UNCHANGED <<slots, ctx, op, entry, secret, dirty, sentMeta,
                             lastStop, snap>>
         [] ~stoppable ->
              \* share and type_info updated, NOT_RUNNING returned.
              /\ share' = [share EXCEPT ![e] = s]
              /\ slots' = [slots EXCEPT ![e] = sl]
              /\ UNCHANGED <<state, nthread, ctx, op, entry, secret, dirty,
                             sentMeta, lastStop, snap>>
         [] OTHER ->
              /\ Dec(e)
              /\ slots'    = [slots EXCEPT ![e] = sl]
              /\ share'    = [share EXCEPT ![e] = s]
              /\ ToSwitch(h, e)
              /\ StopGhosts(e, s, Kind(s))
              /\ sentMeta' = IF s = "RW" THEN [sentMeta EXCEPT ![e] = Range(p)]
                             ELSE sentMeta
              /\ UNCHANGED entry
    /\ UNCHANGED <<hsm, mem, dead>>

-----------------------------------------------------------------------------
(* destroy_enclave                                                         *)

DestroyBegin(h, e) ==
    /\ HostReady(h)
    \* ENCLAVE_EXISTS(eid) && state <= STOPPED
    /\ state[e] \in (IF FixDestroyGuard THEN {"FRESH", "STOPPED"}
                     ELSE {"DESTROYING", "FRESH", "STOPPED"})
    /\ ~(FixStopping /\ Stopping(e))
    /\ state' = [state EXCEPT ![e] = "DESTROYING"]
    /\ ctx'   = [ctx EXCEPT ![h] = Sm(e)]
    /\ op'    = [op EXCEPT ![h] = "d_zero"]
    /\ UNCHANGED <<nthread, share, slots, hsm, entry, mem, secret, dirty,
                   sentMeta, dead, lastStop, snap>>

\* 1.a clear all pages
DestroyZero(h, e) ==
    /\ ctx[h] = Sm(e) /\ op[h] = "d_zero"
    /\ mem'    = [mem EXCEPT ![e] = Zero]
    /\ secret' = [secret EXCEPT ![e] = FALSE]
    /\ op'     = [op EXCEPT ![h] = "d_unset"]
    /\ UNCHANGED <<state, nthread, share, slots, ctx, hsm, entry, dirty,
                   sentMeta, dead, lastStop, snap>>

\* 1.b pmp_unset_global + pmp_region_free_atomic: the region goes back to
\* the host.
DestroyUnset(h, e) ==
    /\ ctx[h] = Sm(e) /\ op[h] = "d_unset"
    /\ entry' = SetG(e, "UNSET")
    /\ RecG(e, "UNSET")
    /\ op'    = [op EXCEPT ![h] = "d_reset"]
    /\ UNCHANGED <<state, nthread, share, slots, ctx, hsm, mem, secret, dirty,
                   sentMeta, dead, lastStop, snap>>

\* 2-3. reset the metadata and encl_free_eid (state := INVALID).
DestroyReset(h, e) ==
    /\ ctx[h] = Sm(e) /\ op[h] = "d_reset"
    /\ state'    = [state EXCEPT ![e] = "INVALID"]
    /\ nthread'  = [nthread EXCEPT ![e] = 0]
    /\ share'    = [share EXCEPT ![e] = "NONE"]
    /\ slots'    = [slots EXCEPT ![e] = NoSlots]
    /\ ctx'      = [ctx EXCEPT ![h] = Host]
    /\ op'       = [op EXCEPT ![h] = "none"]
    /\ dirty'    = [dirty EXCEPT ![e] = FALSE]
    /\ sentMeta' = [sentMeta EXCEPT ![e] = {}]
    /\ lastStop' = [lastStop EXCEPT ![e] = "none"]
    /\ UNCHANGED <<hsm, entry, mem, secret, dead, snap>>

-----------------------------------------------------------------------------
(* HSM.  hart_stop and hart_suspend are issued by the host on the hart     *)
(* itself; hart_start by any started host hart.  A restarted hart goes     *)
(* through OpenSBI warm boot (init_warm_startup -> final_init ->           *)
(* sm_init(false) -> pmp_init), which programs only the SM and OS regions; *)
(* a hart woken from non-retentive suspend goes through init_warm_resume,  *)
(* whose sbi_hart_pmp_configure puts the root domain's "all memory" region *)
(* above every SM entry and never calls final_init.                        *)

\* Entries programmed from the records (#7 fix).
Restored(h) == [e \in EIDs |-> [entry[e] EXCEPT ![h] = rec[e]]]
Cleared(h)  == [e \in EIDs |-> [entry[e] EXCEPT ![h] = "UNSET"]]

HsmOnly(h, st) ==
    /\ hsm' = [hsm EXCEPT ![h] = st]
    /\ UNCHANGED <<state, nthread, share, slots, ctx, op, rec, mem,
                   secret, dirty, sentMeta, dead, lastStop, snap>>

HartStop(h) ==
    /\ HostReady(h)
    /\ Cardinality(Started) > 1
    /\ HsmOnly(h, "STOPPED") /\ UNCHANGED entry

\* hart_start: init_warm_startup (domain PMP, then sm_init -> pmp_init).
HartStart(h) ==
    /\ hsm[h] = "STOPPED"
    /\ \E h2 \in Harts : HostReady(h2)
    /\ HsmOnly(h, "PENDING") /\ entry' = Cleared(h)

\* final_init -> sm_init(false), still START_PENDING.
HartInit(h) ==
    /\ hsm[h] = "PENDING"
    /\ HsmOnly(h, "PENDING_R")
    /\ entry' = IF FixHartRestart /\ RestoreAt \in {"init", "init_resend"}
                THEN Restored(h) ELSE entry

\* sbi_hsm_prepare_next_jump: START_PENDING -> STARTED.
HartStarted(h) ==
    /\ hsm[h] = "PENDING_R"
    /\ HsmOnly(h, "STARTING") /\ UNCHANGED entry

HartSuspend(h) ==
    /\ HostReady(h)
    /\ Cardinality(Started) > 1
    /\ HsmOnly(h, "SUSPENDED") /\ UNCHANGED entry

\* Retentive suspend: PMP kept.
HartWakeRet(h) ==
    /\ hsm[h] = "SUSPENDED"
    /\ HsmOnly(h, "STARTED") /\ UNCHANGED entry

\* Non-retentive suspend: init_warm_resume -> sbi_hart_pmp_configure puts
\* the root domain's "all memory" region above every SM entry.  The SM's
\* HSM device (FixNonRetSuspend) refuses such a suspend with an error other
\* than SBI_ENOTSUPP, so OpenSBI does not fall back to its default.
HartWakeNonRet(h) ==
    /\ ~FixNonRetSuspend
    /\ hsm[h] = "SUSPENDED"
    /\ HsmOnly(h, "RESUMING") /\ entry' = Cleared(h)

\* sbi_hsm_hart_resume_finish: -> STARTED.
HartResumed(h) ==
    /\ hsm[h] = "RESUMING"
    /\ HsmOnly(h, "STARTING") /\ UNCHANGED entry

\* Last M-mode step before sbi_hart_switch_mode (a hook here is the
\* "started" variant of the fix).
HartEnterS(h) ==
    /\ hsm[h] = "STARTING"
    /\ HsmOnly(h, "STARTED")
    /\ entry' = IF FixHartRestart /\ RestoreAt = "started"
                THEN Restored(h) ELSE entry

-----------------------------------------------------------------------------
(* Memory accesses                                                         *)

Adversarial(h, e) ==
    /\ hsm[h] = "STARTED"
    /\ \/ ctx[h] = Host
       \/ \E e2 \in EIDs \ {e} : ctx[h] = Enc(e2)

EncWrite(h, e, d) ==
    /\ ctx[h] = Enc(e)
    /\ Eff(e, h) = "RW"
    /\ mem'    = [mem EXCEPT ![e] = d]
    /\ secret' = [secret EXCEPT ![e] = TRUE]
    /\ UNCHANGED <<state, nthread, share, slots, ctx, op, hsm, entry, dirty,
                   sentMeta, dead, lastStop, snap>>

AdvWrite(h, e, d) ==
    /\ Adversarial(h, e)
    /\ state[e] # "INVALID"
    /\ Eff(e, h) = "RW"
    /\ mem'    = [mem EXCEPT ![e] = d]
    /\ secret' = [secret EXCEPT ![e] = FALSE]
    /\ dirty'  = [dirty EXCEPT ![e] = TRUE]
    /\ UNCHANGED <<state, nthread, share, slots, ctx, op, hsm, entry,
                   sentMeta, dead, lastStop, snap>>

-----------------------------------------------------------------------------

Next ==
    \/ \E h \in Harts, e \in EIDs :
        \/ \E d \in Data, m0 \in Meta : Create(h, e, d, m0)
        \/ (RunBegin(h, e) \/ RunValidate(h, e)) /\ UNCHANGED rec
        \/ (ResumeBegin(h, e) \/ ResumeValidate(h, e)) /\ UNCHANGED rec
        \/ ResumeRevoke(h, e)
        \/ StopInvoluntary(h, e) \/ Exit(h, e)
        \/ StopSwitch(h, e)
        \/ \E s \in ShareV : (\/ StopShare(h, e, s, TRUE, <<>>)
                              \/ \E p \in Prov : StopShare(h, e, s, FALSE, p))
                             /\ UNCHANGED rec
        \/ (DestroyBegin(h, e) \/ DestroyZero(h, e) \/ DestroyReset(h, e))
              /\ UNCHANGED rec
        \/ DestroyUnset(h, e)
        \/ (\E d \in Data : EncWrite(h, e, d) \/ AdvWrite(h, e, d)) /\ UNCHANGED rec
    \/ \E h \in Harts :
        \/ HartStop(h) \/ HartStart(h) \/ HartInit(h) \/ HartStarted(h)
        \/ HartSuspend(h) \/ HartWakeRet(h) \/ HartWakeNonRet(h)
        \/ HartResumed(h) \/ HartEnterS(h)

Spec == Init /\ [][Next]_vars

-----------------------------------------------------------------------------
(* Properties of MODEL.adoc Section 7, restated on the implementation      *)
(* variables.  Names and meaning follow ADM.tla; pmp[e][h] is replaced by  *)
(* the effective permission Eff(e, h).                                     *)

\* The enclave holds its ADM region: from creation until a destroyer
\* unsets the entry (the two last destroy steps correspond to the atomic
\* DestroyFree of ADM.tla).  E1 and A9 cover the released region.
Released(e) == \E h \in Harts : ctx[h] = Sm(e) /\ op[h] = "d_reset"
Exists(e) == state[e] # "INVALID" /\ ~Released(e)
Validating(h, e) == ctx[h] = Sm(e) /\ op[h] \in {"run", "resume_validate"}
Entering(h, e) == ctx[h] = Sm(e) /\ op[h] \in {"run", "resume_revoke", "resume_validate"}

A1_InitialProtection ==
    \A e \in EIDs : state[e] = "FRESH" => \A h \in Harts : Eff(e, h) = "NONE"

A2_ExclusiveWhileRunning ==
    \A e \in EIDs, h \in Harts :
        ctx[h] = Enc(e) =>
            /\ Eff(e, h) = "RW"
            /\ \A h2 \in Harts \ {h} : Eff(e, h2) = "NONE"

A3_OnlyExplicitGrant ==
    \A e \in EIDs, h \in Harts :
        (ctx[h] # Enc(e) /\ Exists(e)) => PermLeq(Eff(e, h), HostPerm(share[e]))

A4_RevokedBeforeEnter ==
    [][\A e \in EIDs, h \in Harts :
          (ctx[h] # Enc(e) /\ ctx'[h] = Enc(e)) =>
              \A h2 \in Harts : Eff(e, h2) = "NONE"]_vars

A6_IsolatedWhenNotShared ==
    \A e \in EIDs, h \in Harts :
        (Exists(e) /\ HostPerm(share[e]) = "NONE" /\ Eff(e, h) # "NONE")
            => ctx[h] = Enc(e)

A8_InvoluntaryStopSafe ==
    \A e \in EIDs :
        (state[e] = "STOPPED" /\ lastStop[e] \in {"invol", "exit"}) =>
            /\ HostPerm(share[e]) = "NONE"
            /\ \A h \in Harts : Eff(e, h) = "NONE"

A9_NoSecretAtRelease ==
    [][\A e \in EIDs :
          (state[e] = "DESTROYING" /\ state'[e] = "INVALID") => ~secret[e]]_vars

E1_NoUnintendedDisclosure ==
    \A e \in EIDs, h \in Harts :
        (secret[e] /\ Adversarial(h, e)) => Eff(e, h) = "NONE"

B1_ValidatedBeforeRun ==
    \A e \in EIDs, h \in Harts : ctx[h] = Enc(e) => ~dirty[e]

B2_NoAccessDuringValidation ==
    \A e \in EIDs, h \in Harts :
        (Validating(h, e) /\ share[e] = "RW") => \A h2 \in Harts : Eff(e, h2) = "NONE"

\* Design form: an enclave that failed validation is destroyed.
B3_FailClosed ==
    \A e \in EIDs :
        dead[e] =>
            /\ state[e] \in {"DESTROYING", "INVALID"}
            /\ \A h \in Harts : ctx[h] # Enc(e)

\* Weaker form: it never executes again.
B3_NeverRunsAgain ==
    \A e \in EIDs : dead[e] => \A h \in Harts : ctx[h] # Enc(e)

B4_FreshMetadata ==
    \A e \in EIDs, h \in Harts :
        (Validating(h, e) /\ share[e] = "RW") => Used(slots[e]) = sentMeta[e]

B5_EnterOnlyViaValidation ==
    [][\A e \in EIDs, h \in Harts :
          (ctx[h] # Enc(e) /\ ctx'[h] = Enc(e)) =>
              /\ Validating(h, e)
              /\ share[e] = "RW" => mem[e] \in sentMeta[e]]_vars

E2_ContentsPreserved ==
    \A e \in EIDs :
        (/\ state[e] \in {"STOPPED", "RUNNING"}
         /\ lastStop[e] \in {"share_none", "share_ro", "invol", "exit"}
         /\ \A h \in Harts : ctx[h] # Enc(e)) => mem[e] = snap[e]

D2d_SingleRunner ==
    \A e \in EIDs :
        Cardinality({h \in Harts : ctx[h] = Enc(e) \/ Entering(h, e)}) <= 1

\* D3: at most one hart destroys a given enclave.
D3_SingleDestroyer ==
    \A e \in EIDs :
        Cardinality({h \in Harts : ctx[h] = Sm(e) /\ op[h] \in {"d_zero", "d_unset", "d_reset"}}) <= 1

-----------------------------------------------------------------------------
(* Reachability witnesses (expected to be VIOLATED in every configuration) *)

Reach_ResumedAfterRWShare ==
    ~ \E e \in EIDs, h \in Harts : ctx[h] = Enc(e) /\ lastStop[e] = "share_rw"

Reach_RunsWhileHartStopped ==
    ~ \E e \in EIDs, h, h2 \in Harts : ctx[h] = Enc(e) /\ hsm[h2] = "STOPPED"

Reach_ValidationFails ==
    ~ \E e \in EIDs : dead[e]

\* A hart is restarting while the ADM is shared with the host.
Reach_RestartWhileShared ==
    ~ \E e \in EIDs, h \in Harts : hsm[h] = "PENDING_R" /\ rec[e] = "RW"

\* A hart is in context_switch_to_host after the lock was dropped.
Reach_StopSwitchPending ==
    ~ \E h \in Harts : op[h] = "stop_switch"

=============================================================================
