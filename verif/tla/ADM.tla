-------------------------------- MODULE ADM --------------------------------
(***************************************************************************)
(* Design model of Additional Data Memory (ADM) / Protected Sharing.       *)
(*                                                                         *)
(* This module transcribes Sections 3-5 of verif/docs/MODEL.typ and        *)
(* states the access-control properties (group A, Section 7.1) together    *)
(* with the state-consistency invariants (D2) they rely on.                *)
(*                                                                         *)
(* Abstractions made here, beyond those of MODEL.typ:                      *)
(*  - Region placement (group C) is not modeled; an ADM exists exactly     *)
(*    while state[e] # "INVALID".                                          *)
(*  - The ADM contents are a single abstract value from Data, and the      *)
(*    structural predicate WF(mem, meta) is abstracted to "mem = meta".    *)
(*  - Reads are not modeled (they change no state); they matter for the    *)
(*    confidentiality property E1, which is out of scope of this module.   *)
(*  - The execute permission is not in Perm, so A5 holds by construction.  *)
(***************************************************************************)
EXTENDS Naturals, FiniteSets

CONSTANTS
    Harts,      \* set of harts
    EIDs,       \* set of enclave IDs
    Data,       \* abstract ADM contents
    Zero,       \* the all-zero contents
    Meta,       \* expected metadata values
    NoE,        \* "no enclave"
    NoH,        \* "no hart"
    NoMeta      \* "no metadata"

ASSUME /\ Zero \in Data
       /\ Meta \subseteq Data \ {Zero}
       /\ NoE \notin EIDs
       /\ NoH \notin Harts
       /\ NoMeta \notin Meta

VARIABLES
    state,      \* state[e]   : lifecycle state of enclave e
    share,      \* share[e]   : sharing state (permission granted to the host)
    meta,       \* meta[e]    : expected metadata for the next validation
    phase,      \* phase[e]   : SM-internal progress of run / resume / destroy
    runner,     \* runner[e]  : hart executing e, or processing run / resume
    ctx,        \* ctx[h]     : context of hart h
    pmp,        \* pmp[e][h]  : S/U-mode permission of hart h on e's ADM
    mem,        \* mem[e]     : contents of e's ADM
    \* ghost variables
    secret,     \* secret[e]  : the contents hold data the enclave has not disclosed
    lastStop,   \* lastStop[e]: how e was stopped most recently
    dirty,      \* dirty[e]   : a non-owner could have written since the last validation
    sentMeta,   \* sentMeta[e]: metadata most recently supplied by a legitimate source
    dead,       \* dead[e]    : validation failed for this incarnation of e
    snap        \* snap[e]    : contents of the ADM at the most recent stop

vars == <<state, share, meta, phase, runner, ctx, pmp, mem, secret, lastStop,
         dirty, sentMeta, dead, snap>>

Perm     == {"NONE", "R", "RW"}
Share    == {"NONE", "RONLY", "RW"}
State    == {"INVALID", "FRESH", "RUNNING", "STOPPED", "DESTROYING"}
Phase    == {"IDLE", "REVOKING", "VALIDATING", "ZEROED"}
StopKind == {"none", "share_none", "share_ro", "share_rw", "invol", "exit"}

Host   == <<"HOST", NoE>>
Enc(e) == <<"ENC", e>>
Sm(e)  == <<"SM", e>>
Ctx    == {Host} \cup {Enc(e) : e \in EIDs} \cup {Sm(e) : e \in EIDs}

PermRank(p) == CASE p = "NONE" -> 0
                 [] p = "R"    -> 1
                 [] p = "RW"   -> 2
PermLeq(p, q) == PermRank(p) <= PermRank(q)

HostPerm(s) == CASE s = "NONE"  -> "NONE"
                 [] s = "RONLY" -> "R"
                 [] s = "RW"    -> "RW"

\* Abstract structural predicate (MODEL.typ Section 6.2).
WF(d, m) == d = m

NoPerm == [h \in Harts |-> "NONE"]

TypeOK ==
    /\ state    \in [EIDs -> State]
    /\ share    \in [EIDs -> Share]
    /\ meta     \in [EIDs -> Meta \cup {NoMeta}]
    /\ phase    \in [EIDs -> Phase]
    /\ runner   \in [EIDs -> Harts \cup {NoH}]
    /\ ctx      \in [Harts -> Ctx]
    /\ pmp      \in [EIDs -> [Harts -> Perm]]
    /\ mem      \in [EIDs -> Data]
    /\ secret   \in [EIDs -> BOOLEAN]
    /\ lastStop \in [EIDs -> StopKind]
    /\ dirty    \in [EIDs -> BOOLEAN]
    /\ sentMeta \in [EIDs -> Meta \cup {NoMeta}]
    /\ dead     \in [EIDs -> BOOLEAN]
    /\ snap     \in [EIDs -> Data]

Init ==
    /\ state    = [e \in EIDs |-> "INVALID"]
    /\ share    = [e \in EIDs |-> "NONE"]
    /\ meta     = [e \in EIDs |-> NoMeta]
    /\ phase    = [e \in EIDs |-> "IDLE"]
    /\ runner   = [e \in EIDs |-> NoH]
    /\ ctx      = [h \in Harts |-> Host]
    /\ pmp      = [e \in EIDs |-> NoPerm]
    /\ mem      = [e \in EIDs |-> Zero]
    /\ secret   = [e \in EIDs |-> FALSE]
    /\ lastStop = [e \in EIDs |-> "none"]
    /\ dirty    = [e \in EIDs |-> FALSE]
    /\ sentMeta = [e \in EIDs |-> NoMeta]
    /\ dead     = [e \in EIDs |-> FALSE]
    /\ snap     = [e \in EIDs |-> Zero]

-----------------------------------------------------------------------------
(* SM operations (MODEL.typ Section 5.1)                                   *)

\* The host creates enclave e with initial ADM contents d and metadata m0.
Create(h, e, d, m0) ==
    /\ ctx[h] = Host
    /\ state[e] = "INVALID"
    /\ state'    = [state EXCEPT ![e] = "FRESH"]
    /\ share'    = [share EXCEPT ![e] = "RW"]
    /\ meta'     = [meta EXCEPT ![e] = m0]
    /\ pmp'      = [pmp EXCEPT ![e] = NoPerm]
    /\ mem'      = [mem EXCEPT ![e] = d]
    /\ secret'   = [secret EXCEPT ![e] = FALSE]
    /\ lastStop' = [lastStop EXCEPT ![e] = "none"]
    \* The host initialized the ADM, so it must be validated before the first run.
    /\ dirty'    = [dirty EXCEPT ![e] = TRUE]
    /\ sentMeta' = [sentMeta EXCEPT ![e] = m0]
    /\ dead'     = [dead EXCEPT ![e] = FALSE]
    /\ UNCHANGED <<phase, runner, ctx, snap>>

RunBegin(h, e) ==
    /\ ctx[h] = Host
    /\ state[e] = "FRESH"
    /\ state'  = [state EXCEPT ![e] = "RUNNING"]
    /\ ctx'    = [ctx EXCEPT ![h] = Sm(e)]
    /\ runner' = [runner EXCEPT ![e] = h]
    /\ phase'  = [phase EXCEPT ![e] = "VALIDATING"]
    /\ UNCHANGED <<share, meta, pmp, mem, secret, lastStop,
                   dirty, sentMeta, dead, snap>>

ResumeBegin(h, e) ==
    /\ ctx[h] = Host
    /\ state[e] = "STOPPED"
    /\ phase[e] = "IDLE"
    /\ state'  = [state EXCEPT ![e] = "RUNNING"]
    /\ ctx'    = [ctx EXCEPT ![h] = Sm(e)]
    /\ runner' = [runner EXCEPT ![e] = h]
    /\ phase'  = [phase EXCEPT ![e] = "REVOKING"]
    /\ UNCHANGED <<share, meta, pmp, mem, secret, lastStop,
                   dirty, sentMeta, dead, snap>>

\* One hart applies the revocation requested through an IPI.
RevokeStep(h, e) ==
    /\ phase[e] = "REVOKING"
    /\ pmp[e][h] # "NONE"
    /\ pmp' = [pmp EXCEPT ![e][h] = "NONE"]
    /\ UNCHANGED <<state, share, meta, phase, runner, ctx, mem, secret, lastStop,
                   dirty, sentMeta, dead, snap>>

RevokeDone(e) ==
    /\ phase[e] = "REVOKING"
    /\ \A h \in Harts : pmp[e][h] = "NONE"
    /\ phase' = [phase EXCEPT ![e] = "VALIDATING"]
    /\ UNCHANGED <<state, share, meta, runner, ctx, pmp, mem, secret, lastStop,
                   dirty, sentMeta, dead, snap>>

\* Validation succeeds (or is not required); the enclave is entered.
ValidatePass(e) ==
    /\ phase[e] = "VALIDATING"
    /\ share[e] # "RW" \/ WF(mem[e], meta[e])
    /\ LET h == runner[e] IN
        /\ pmp' = [pmp EXCEPT ![e][h] = "RW"]
        /\ ctx' = [ctx EXCEPT ![h] = Enc(e)]
    /\ share' = [share EXCEPT ![e] = "NONE"]
    /\ meta'  = [meta EXCEPT ![e] = NoMeta]
    /\ phase' = [phase EXCEPT ![e] = "IDLE"]
    \* dirty is cleared only when the contents were actually checked.
    /\ dirty' = IF share[e] = "RW" THEN [dirty EXCEPT ![e] = FALSE] ELSE dirty
    /\ UNCHANGED <<state, runner, mem, secret, lastStop, sentMeta, dead, snap>>

\* Validation fails; the SM starts destroying the enclave.
ValidateFail(e) ==
    /\ phase[e] = "VALIDATING"
    /\ share[e] = "RW"
    /\ ~WF(mem[e], meta[e])
    /\ ctx'    = [ctx EXCEPT ![runner[e]] = Host]
    /\ runner' = [runner EXCEPT ![e] = NoH]
    /\ phase'  = [phase EXCEPT ![e] = "IDLE"]
    /\ state'  = [state EXCEPT ![e] = "DESTROYING"]
    /\ dead'   = [dead EXCEPT ![e] = TRUE]
    /\ UNCHANGED <<share, meta, pmp, mem, secret, lastStop, dirty, sentMeta, snap>>

\* Enclave e, running on hart h, stops with sharing state s and metadata m.
Stop(h, e, s, m, kind) ==
    /\ ctx[h] = Enc(e)
    /\ state[e] = "RUNNING"
    /\ state'    = [state EXCEPT ![e] = "STOPPED"]
    /\ share'    = [share EXCEPT ![e] = s]
    /\ meta'     = [meta EXCEPT ![e] = m]
    /\ ctx'      = [ctx EXCEPT ![h] = Host]
    /\ runner'   = [runner EXCEPT ![e] = NoH]
    /\ pmp'      = IF s = "NONE"
                   THEN [pmp EXCEPT ![e][h] = "NONE"]
                   ELSE [pmp EXCEPT ![e] = [h2 \in Harts |-> HostPerm(s)]]
    /\ lastStop' = [lastStop EXCEPT ![e] = kind]
    \* Sharing discloses the current contents by the enclave's own intent.
    /\ secret'   = IF s = "NONE" THEN secret ELSE [secret EXCEPT ![e] = FALSE]
    \* A writable share lets non-owners write, and supplies fresh metadata.
    /\ dirty'    = IF s = "RW" THEN [dirty EXCEPT ![e] = TRUE] ELSE dirty
    /\ sentMeta' = IF s = "RW" THEN [sentMeta EXCEPT ![e] = m] ELSE sentMeta
    /\ snap'     = [snap EXCEPT ![e] = mem[e]]
    /\ UNCHANGED <<phase, mem, dead>>

\* Edge call: the enclave chooses the sharing state; metadata iff RW.
StopShare(h, e) ==
    \E s \in Share, m \in Meta \cup {NoMeta} :
        /\ (s = "RW") <=> (m \in Meta)
        /\ Stop(h, e, s, m, CASE s = "NONE"  -> "share_none"
                              [] s = "RONLY" -> "share_ro"
                              [] s = "RW"    -> "share_rw")

\* Timer interrupt etc.: no sharing request.
StopInvoluntary(h, e) == Stop(h, e, "NONE", NoMeta, "invol")

Exit(h, e) == Stop(h, e, "NONE", NoMeta, "exit")

DestroyBegin(h, e) ==
    /\ ctx[h] = Host
    /\ state[e] \in {"FRESH", "STOPPED"}
    /\ phase[e] = "IDLE"
    /\ state' = [state EXCEPT ![e] = "DESTROYING"]
    /\ UNCHANGED <<share, meta, phase, runner, ctx, pmp, mem, secret, lastStop,
                   dirty, sentMeta, dead, snap>>

\* DestroyBody step 1: zero the ADM.
DestroyZero(e) ==
    /\ state[e] = "DESTROYING"
    /\ phase[e] = "IDLE"
    /\ mem'     = [mem EXCEPT ![e] = Zero]
    /\ secret' = [secret EXCEPT ![e] = FALSE]
    /\ phase'   = [phase EXCEPT ![e] = "ZEROED"]
    /\ UNCHANGED <<state, share, meta, runner, ctx, pmp, lastStop,
                   dirty, sentMeta, dead, snap>>

\* DestroyBody steps 2-3: release the PMP entry and reset the SM state.
DestroyFree(e) ==
    /\ state[e] = "DESTROYING"
    /\ phase[e] = "ZEROED"
    /\ state'    = [state EXCEPT ![e] = "INVALID"]
    /\ pmp'      = [pmp EXCEPT ![e] = NoPerm]
    /\ share'    = [share EXCEPT ![e] = "NONE"]
    /\ meta'     = [meta EXCEPT ![e] = NoMeta]
    /\ phase'    = [phase EXCEPT ![e] = "IDLE"]
    /\ lastStop' = [lastStop EXCEPT ![e] = "none"]
    /\ dirty'    = [dirty EXCEPT ![e] = FALSE]
    /\ sentMeta' = [sentMeta EXCEPT ![e] = NoMeta]
    /\ UNCHANGED <<runner, ctx, mem, secret, dead, snap>>

-----------------------------------------------------------------------------
(* Memory accesses (MODEL.typ Sections 5.2 and 5.3)                        *)

\* Hart h is adversarial with respect to e: host, or another enclave.
Adversarial(h, e) == ctx[h] = Host \/ \E e2 \in EIDs \ {e} : ctx[h] = Enc(e2)

EncWrite(h, e, d) ==
    /\ ctx[h] = Enc(e)
    /\ pmp[e][h] = "RW"
    /\ mem'     = [mem EXCEPT ![e] = d]
    /\ secret' = [secret EXCEPT ![e] = TRUE]
    /\ UNCHANGED <<state, share, meta, phase, runner, ctx, pmp, lastStop,
                   dirty, sentMeta, dead, snap>>

AdvWrite(h, e, d) ==
    /\ Adversarial(h, e)
    /\ state[e] # "INVALID"
    /\ pmp[e][h] = "RW"
    /\ mem'     = [mem EXCEPT ![e] = d]
    /\ secret' = [secret EXCEPT ![e] = FALSE]
    /\ dirty'   = [dirty EXCEPT ![e] = TRUE]
    /\ UNCHANGED <<state, share, meta, phase, runner, ctx, pmp, lastStop, sentMeta, dead, snap>>

-----------------------------------------------------------------------------

\* Steps taken by hart h concerning enclave e, other than Create.
HartStep(h, e) ==
    \/ RunBegin(h, e)
    \/ ResumeBegin(h, e)
    \/ RevokeStep(h, e)
    \/ StopShare(h, e)
    \/ StopInvoluntary(h, e)
    \/ Exit(h, e)
    \/ DestroyBegin(h, e)
    \/ \E d \in Data : EncWrite(h, e, d) \/ AdvWrite(h, e, d)

\* SM-internal steps concerning enclave e, other than DestroyFree.
InternalStep(e) ==
    \/ RevokeDone(e)
    \/ ValidatePass(e)
    \/ ValidateFail(e)
    \/ DestroyZero(e)

Next ==
    \/ \E h \in Harts, e \in EIDs :
        \/ \E d \in Data, m0 \in Meta : Create(h, e, d, m0)
        \/ HartStep(h, e)
    \/ \E e \in EIDs : InternalStep(e) \/ DestroyFree(e)

Spec == Init /\ [][Next]_vars

-----------------------------------------------------------------------------
(* Group A: access control (MODEL.typ Section 7.1)                         *)

\* A1: right after creation, no hart can access the ADM.
A1_InitialProtection ==
    \A e \in EIDs : state[e] = "FRESH" => \A h \in Harts : pmp[e][h] = "NONE"

\* A2: while running, only the executing hart can access the ADM.
A2_ExclusiveWhileRunning ==
    \A e \in EIDs, h \in Harts :
        ctx[h] = Enc(e) =>
            /\ pmp[e][h] = "RW"
            /\ \A h2 \in Harts \ {h} : pmp[e][h2] = "NONE"

\* A3: nobody but the owner exceeds the sharing state chosen by the enclave.
A3_OnlyExplicitGrant ==
    \A e \in EIDs, h \in Harts :
        (ctx[h] # Enc(e) /\ state[e] # "INVALID") =>
            PermLeq(pmp[e][h], HostPerm(share[e]))

\* A4: the enclave is entered only when no hart holds a permission.
A4_RevokedBeforeEnter ==
    [][\A e \in EIDs, h \in Harts :
          (ctx[h] # Enc(e) /\ ctx'[h] = Enc(e)) =>
              \A h2 \in Harts : pmp[e][h2] = "NONE"]_vars

\* A5 (no execute permission) holds by construction: Perm has no X.

\* A6: when not shared, only the owner enclave can access the ADM.
A6_IsolatedWhenNotShared ==
    \A e \in EIDs, h \in Harts :
        (share[e] = "NONE" /\ pmp[e][h] # "NONE") => ctx[h] = Enc(e)

\* A7: the sharing state and the metadata change only through Create, a
\* stop issued by the owner, the SM's validation, or destruction.
A7_ShareStateIntegrity ==
    [][\A e \in EIDs :
          (share'[e] # share[e] \/ meta'[e] # meta[e]) =>
              \/ state[e] = "INVALID" /\ state'[e] = "FRESH"
              \/ \E h \in Harts : ctx[h] = Enc(e) /\ ctx'[h] = Host
              \/ phase[e] = "VALIDATING"
                   /\ \E h \in Harts : ctx[h] = Sm(e) /\ ctx'[h] = Enc(e)
              \/ state[e] = "DESTROYING" /\ state'[e] = "INVALID"]_vars

\* A8: a stop without a sharing request grants nothing to the host.
A8_InvoluntaryStopSafe ==
    \A e \in EIDs :
        (state[e] = "STOPPED" /\ lastStop[e] \in {"invol", "exit"}) =>
            /\ share[e] = "NONE"
            /\ \A h \in Harts : pmp[e][h] = "NONE"

\* A9: when the region is released, it holds nothing the enclave keeps
\* confidential.  This is the release-time case of E1 below.
A9_NoSecretAtRelease ==
    [][\A e \in EIDs :
          (state[e] = "DESTROYING" /\ state'[e] = "INVALID") => ~secret[e]]_vars

\* A stronger form that does NOT hold: the contents are zero at release.
\* If the enclave is destroyed while shared as RW, the host can write
\* between zeroing and release.  Kept to document the counterexample.
A9_Strict ==
    [][\A e \in EIDs :
          (state[e] = "DESTROYING" /\ state'[e] = "INVALID") => mem[e] = Zero]_vars

-----------------------------------------------------------------------------
(* E1 (state form): the ADM contents are never disclosed without the       *)
(* enclave's intent.  While the contents are confidential, no adversarial  *)
(* hart can read them; A9 covers the moment the region is released.        *)

E1_NoUnintendedDisclosure ==
    \A e \in EIDs, h \in Harts :
        (secret[e] /\ Adversarial(h, e)) => pmp[e][h] = "NONE"

-----------------------------------------------------------------------------
(* Group B: enforcement of validation (MODEL.typ Section 7.2)              *)
(* B6 and B7 concern the validation function itself and are checked at     *)
(* the code level, not in this model.                                      *)

\* B1: once a non-owner could have written, the enclave runs only after
\* a successful validation.
B1_ValidatedBeforeRun ==
    \A e \in EIDs, h \in Harts : ctx[h] = Enc(e) => ~dirty[e]

\* B2 / B8: from the start of validation until the enclave is entered, no
\* hart has S/U-mode access; only the SM can touch the ADM.
B2_NoAccessDuringValidation ==
    \A e \in EIDs :
        phase[e] = "VALIDATING" => \A h \in Harts : pmp[e][h] = "NONE"

\* B3: an enclave that failed validation never executes again.
B3_FailClosed ==
    \A e \in EIDs :
        dead[e] =>
            /\ state[e] \in {"DESTROYING", "INVALID"}
            /\ \A h \in Harts : ctx[h] # Enc(e)

\* B4: validation uses the metadata most recently supplied by a
\* legitimate source (Create, or the owner's writable stop).
B4_FreshMetadata ==
    \A e \in EIDs :
        (phase[e] = "VALIDATING" /\ share[e] = "RW") => meta[e] = sentMeta[e]

\* B5: a fresh enclave is always pending validation.
B5_FreshNeedsValidation ==
    \A e \in EIDs : state[e] = "FRESH" => (share[e] = "RW" /\ dirty[e])

\* B1 / B5 (action form): the enclave is entered only out of the validation
\* phase, and after a writable share only with well-formed contents.
B5_EnterOnlyViaValidation ==
    [][\A e \in EIDs, h \in Harts :
          (ctx[h] # Enc(e) /\ ctx'[h] = Enc(e)) =>
              /\ phase[e] = "VALIDATING"
              /\ share[e] = "RW" => WF(mem[e], sentMeta[e])]_vars

\* E2 (first half): unless the enclave granted write access, the contents
\* it finds on resumption are the ones it left.  The second half (contents
\* after a writable share are well-formed) is B5_EnterOnlyViaValidation.
E2_ContentsPreserved ==
    \A e \in EIDs :
        (/\ state[e] \in {"STOPPED", "RUNNING"}
         /\ lastStop[e] \in {"share_none", "share_ro", "invol", "exit"}
         /\ \A h \in Harts : ctx[h] # Enc(e)) => mem[e] = snap[e]

-----------------------------------------------------------------------------
(* D2: state consistency (MODEL.typ Section 7.4)                           *)

D2a_RunningConsistent ==
    \A e \in EIDs, h \in Harts :
        ctx[h] = Enc(e) =>
            /\ state[e] = "RUNNING"
            /\ phase[e] = "IDLE"
            /\ runner[e] = h
            /\ share[e] = "NONE"
            /\ meta[e] = NoMeta

\* D1: an enclave is stopped only by itself, and started only by the host.
\* (Create and DestroyBegin are host-only by their guards.)
D1_CallerRestriction ==
    [][\A e \in EIDs :
          /\ (state[e] = "RUNNING" /\ state'[e] = "STOPPED") =>
                \E h \in Harts : ctx[h] = Enc(e) /\ ctx'[h] = Host
          /\ (state[e] \in {"FRESH", "STOPPED"} /\ state'[e] = "RUNNING") =>
                \E h \in Harts : ctx[h] = Host /\ ctx'[h] = Sm(e)]_vars

\* D2(b): while a writable share is pending, the stored metadata is the
\* one that was supplied.
D2b_MetaMatchesSent ==
    \A e \in EIDs :
        (share[e] = "RW" /\ state[e] # "INVALID") => meta[e] = sentMeta[e]

D2c_InvalidConsistent ==
    \A e \in EIDs :
        state[e] = "INVALID" =>
            /\ share[e] = "NONE"
            /\ meta[e] = NoMeta
            /\ \A h \in Harts : pmp[e][h] = "NONE"

D2d_SingleRunner ==
    \A e \in EIDs :
        Cardinality({h \in Harts : ctx[h] \in {Enc(e), Sm(e)}}) <= 1

-----------------------------------------------------------------------------
(* Reachability witnesses.  Each is the negation of a situation the model  *)
(* is expected to reach; TLC must report every one of them as violated.    *)
(* They guard against a model whose guards are so strong that the          *)
(* properties above hold vacuously.                                        *)

\* An enclave runs again after a writable share passed validation.
Reach_ResumedAfterRWShare ==
    ~ \E e \in EIDs, h \in Harts : ctx[h] = Enc(e) /\ lastStop[e] = "share_rw"

\* Two enclaves run at the same time.
Reach_TwoEnclavesRunning ==
    ~ \E e1, e2 \in EIDs, h1, h2 \in Harts :
        e1 # e2 /\ ctx[h1] = Enc(e1) /\ ctx[h2] = Enc(e2)

\* A shared ADM is writable from a hart that runs another enclave.
Reach_SharedVisibleToOtherEnclave ==
    ~ \E e1, e2 \in EIDs, h \in Harts :
        e1 # e2 /\ ctx[h] = Enc(e2) /\ pmp[e1][h] = "RW"

\* Revocation is observed half-way: some harts revoked, some not.
Reach_PartialRevocation ==
    ~ \E e \in EIDs, h1, h2 \in Harts :
        phase[e] = "REVOKING" /\ pmp[e][h1] = "NONE" /\ pmp[e][h2] # "NONE"

\* Validation of a writable share is about to fail.
Reach_ValidationFails ==
    ~ \E e \in EIDs :
        /\ phase[e] = "VALIDATING" /\ share[e] = "RW" /\ lastStop[e] = "share_rw"
        /\ ~WF(mem[e], meta[e])

=============================================================================
