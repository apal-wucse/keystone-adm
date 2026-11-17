----------------------------- MODULE ADMRegions -----------------------------
(***************************************************************************)
(* Region placement and PMP entry priority for the ADM design model        *)
(* (group C, MODEL.adoc Section 7.3).                                      *)
(*                                                                         *)
(* This module extends ADM with the physical placement of each enclave's   *)
(* private memory (EPM) and ADM, and derives the permission a hart         *)
(* actually gets at an address from the PMP entries:                       *)
(*                                                                         *)
(*   priority   entry                 permission on hart h                 *)
(*   highest    SM region             none                                 *)
(*   middle     EPM of enclave e      all if h runs e, otherwise none      *)
(*   middle     ADM of enclave e      pmp[e][h]                            *)
(*   lowest     host OS / UTM         (not modeled; only reached when no   *)
(*                                     entry above matches)                *)
(*                                                                         *)
(* The relative priority of the middle entries is NOT fixed: the           *)
(* properties below quantify over every entry that matches an address, so  *)
(* they hold for any order in which the SM happens to allocate entries.    *)
(*                                                                         *)
(* Abstractions: addresses are a small set of page numbers; the UTM is     *)
(* not placed (it is untrusted memory under the lowest-priority entry);    *)
(* TOR/NAPOT encoding of an entry (C3) is an implementation matter and is  *)
(* represented here only as "an entry matches exactly its region".         *)
(***************************************************************************)
EXTENDS ADM

CONSTANTS
    Addr,       \* set of page numbers (naturals)
    SMRegion    \* pages occupied by the SM

ASSUME /\ Addr \subseteq Nat
       /\ SMRegion \subseteq Addr

VARIABLES
    epm,        \* epm[e] : pages of e's private memory ({} if e does not exist)
    adm         \* adm[e] : pages of e's ADM            ({} if e does not exist)

varsR == <<vars, epm, adm>>

\* Non-empty contiguous ranges of pages.  No alignment or size restriction:
\* the ADM entry uses TOR matching (C4).
Intervals == {{a \in Addr : lo <= a /\ a <= hi} : lo \in Addr, hi \in Addr} \ {{}}

\* C1: the new regions overlap neither each other, the SM, nor any region
\* of an existing enclave.
NoOverlap(e, Re, Ra) ==
    /\ Re \cap Ra = {}
    /\ (Re \cup Ra) \cap SMRegion = {}
    /\ \A e2 \in EIDs \ {e} : (Re \cup Ra) \cap (epm[e2] \cup adm[e2]) = {}

RegionTypeOK ==
    /\ epm \in [EIDs -> Intervals \cup {{}}]
    /\ adm \in [EIDs -> Intervals \cup {{}}]

InitR ==
    /\ Init
    /\ epm = [e \in EIDs |-> {}]
    /\ adm = [e \in EIDs |-> {}]

CreateR(h, e, d, m0, Re, Ra) ==
    /\ NoOverlap(e, Re, Ra)
    /\ Create(h, e, d, m0)
    /\ epm' = [epm EXCEPT ![e] = Re]
    /\ adm' = [adm EXCEPT ![e] = Ra]

\* Releasing the PMP entries returns both regions to the host.
DestroyFreeR(e) ==
    /\ DestroyFree(e)
    /\ epm' = [epm EXCEPT ![e] = {}]
    /\ adm' = [adm EXCEPT ![e] = {}]

NextR ==
    \/ \E h \in Harts, e \in EIDs :
        \/ \E d \in Data, m0 \in Meta, Re \in Intervals, Ra \in Intervals :
              CreateR(h, e, d, m0, Re, Ra)
        \/ HartStep(h, e) /\ UNCHANGED <<epm, adm>>
    \/ \E e \in EIDs :
        \/ InternalStep(e) /\ UNCHANGED <<epm, adm>>
        \/ DestroyFreeR(e)

SpecR == InitR /\ [][NextR]_varsR

-----------------------------------------------------------------------------
(* Permissions derived from the PMP entries                                *)

EpmPerm(h, e) == IF ctx[h] = Enc(e) THEN "ALL" ELSE "NONE"

\* Permissions of every middle-priority entry of hart h that matches page a.
MatchPerms(h, a) ==
    {EpmPerm(h, e) : e \in {x \in EIDs : a \in epm[x]}}
        \cup {pmp[e][h] : e \in {x \in EIDs : a \in adm[x]}}

-----------------------------------------------------------------------------
(* Group C                                                                 *)

\* C1: protected regions are pairwise disjoint.
C1_RegionsDisjoint ==
    /\ \A e \in EIDs :
        /\ epm[e] \cap adm[e] = {}
        /\ (epm[e] \cup adm[e]) \cap SMRegion = {}
    /\ \A e1, e2 \in EIDs :
        e1 # e2 => (epm[e1] \cup adm[e1]) \cap (epm[e2] \cup adm[e2]) = {}

\* Regions exist exactly while the enclave exists.
C_RegionsFollowLifecycle ==
    \A e \in EIDs : (state[e] = "INVALID") <=> (epm[e] = {} /\ adm[e] = {})

\* C2 (consequence): the SM region is matched by no enclave entry, so the
\* highest-priority entry alone decides, and it grants nothing.
C2_SMNotShadowed ==
    \A a \in SMRegion, h \in Harts : MatchPerms(h, a) = {}

\* Consequence of C1 for private memory: whatever the priority order of the
\* middle entries, a hart that does not run e gets no access to e's EPM.
\* In particular, sharing an ADM never exposes another enclave's EPM.
C_EPMIsolated ==
    \A e \in EIDs, h \in Harts : \A a \in epm[e] :
        MatchPerms(h, a) = {EpmPerm(h, e)}

\* Consequence of C1 for the ADM: the permission that applies to every page
\* of e's ADM is exactly pmp[e][h], whatever the priority order.  This
\* lifts the properties of groups A and B from the abstract pmp[e][h] to
\* physical addresses.
C_ADMPermFaithful ==
    \A e \in EIDs, h \in Harts : \A a \in adm[e] :
        MatchPerms(h, a) = {pmp[e][h]}

-----------------------------------------------------------------------------
(* Reachability witness (expected to be VIOLATED)                          *)

IsNapot(R) ==
    LET n  == Cardinality(R)
        lo == CHOOSE a \in R : \A b \in R : a <= b
    IN  /\ n \in {1, 2, 4, 8}
        /\ lo % n = 0

\* C4: an enclave runs with an ADM whose size is not a power of two or
\* whose base is not aligned to its size.
Reach_NonNapotADM ==
    ~ \E e \in EIDs, h \in Harts : ctx[h] = Enc(e) /\ ~IsNapot(adm[e])

=============================================================================
