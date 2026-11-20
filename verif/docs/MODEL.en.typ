#import "model.typ": doc
#import "rocq.typ": rocq-appendix

#show: doc.with(title: [Formal Model of ADM / Protected Sharing], lang: "en")

This document formalizes, as a state transition system, the design in Chapter 3 of the dissertation, "Efficient Large-Scale Data Transfer Approach over the Trust Boundary". It does not depend on any particular verification tool. Machine-readable specifications (TLA+ and others) are to be written separately, with this document as the source of truth.

- Sections 1–7 describe the *design model as stated in the dissertation*.
- Section 8 lists the places where the implementation (`sm/src/`) departs from this model. These are candidates found by reading the code; the effect of six of them was checked with the implementation model of Section 10.3. None has been confirmed on hardware.

Japanese version: `MODEL.typ` (PDF: `MODEL.pdf`). The content is the same; section numbers and IDs are shared.

Notation: `x'` is the value after a transition, `∀h` ranges over all harts, `⊥` means undefined, and `[a, b)` is a half-open interval.

= 1. Scope and Assumptions

== 1.1 In scope

- Protection and validation of the ADM by the Security Monitor (SM).
- The configuration is fixed to `DEFAULT_NOPERM` (the ADM is inaccessible to the host by default).
- Enclaves are single-threaded (`MAX_ENCL_THREADS = 1`).

== 1.2 Out of scope

- The `DEFAULT_READ` mode.
- The internals of the runtime (RT) and the eapp. Both are abstracted as "a principal that may call stop with any sharing state and any metadata". As an exception, adding and removing regions with the enclave-side ADM library is checked for consistency with the validation function (Section 10.4).
- The allocator inside the ADM (`adm_alloc.c`). It is a simple malloc taken from elsewhere, and the validation of the SM does not look at the contents of the allocator's region (block sizes and the free list). The region is treated as such, and the behaviour of the allocator is not guaranteed. Giving that guarantee is a possible extension.
- Denial of service, physical attacks, and side channels (dissertation Section 3.3).
- Attestation, and the protection of private memory (EPM) itself (assumed as properties of the original Keystone).

== 1.3 Adversary (dissertation Section 3.3)
The adversary can:

- control every hart in the host context and read or write any memory that PMP allows;
- issue host SBI calls (create / run / resume / destroy) in any order, with any arguments, from any hart;
- launch malicious enclaves, which issue enclave SBI calls (stop variants / exit) with arbitrary arguments.

== 1.4 Assumptions (taken as given outside the model)
#table(
  columns: (2fr, 12fr),
  table.header([*ID*], [*Assumption*]),
  [H1], [PMP behaves as defined in the RISC-V Privileged Architecture. An S/U-mode access follows the permissions of the lowest-numbered matching entry],
  [H2], [The SM's code and data cannot be read or written from S/U mode (highest-priority PMP entry)],
  [H3], [PMP updates to all harts (via IPI) are synchronous. An update request does not return until every target hart has applied it],
  [H4], [Every hart is an IPI target. A hart that is stopped and started later obtains the correct PMP configuration when it starts],
  [H5], [The SM's own (M-mode) accesses to the ADM are not blocked by PMP],
  [H6], [There is no memory access path that bypasses PMP (such as DMA)],
  [H7], [The enclave issuing an enclave SBI call is identified correctly from the per-hart context information kept by the SM],
  [H8], [The SM reads or writes an address given by the caller of an SBI call only through the PMP check with the privilege of the caller, inside the EPM of the calling enclave, or inside a range the SM has registered as a PMP region (zeroing the UTM and the regions being destroyed, hashing the EPM, validating the ADM)],
)

H3 and H4 are the counterpart of verification item D4: they are assumptions in the design model and proof obligations when verifying the implementation. H8 did not hold in `get_sealing_key`, which has been fixed (Section 8 \#15). For the fixed code it is checked with CBMC (Section 10.4).

= 2. Constants and Sets

#table(
  columns: (2fr, 12fr),
  table.header([*Name*], [*Definition*]),
  [`Harts`], [A finite set of harts (at least two)],
  [`EIDs`], [A finite set of enclave IDs],
  [`Perm`], [`{NONE, R, RW}`, ordered `NONE < R < RW`. The execute permission X is not in the range (A5)],
  [`Share`], [`{NONE, RONLY, RW}`],
  [`hostPerm(s)`], [`NONE → NONE`, `RONLY → R`, `RW → RW`],
  [`State`], [`{INVALID, FRESH, RUNNING, STOPPED, DESTROYING}`],
  [`Phase`], [`{IDLE, REVOKING, VALIDATING}`. Progress of run / resume inside the SM],
  [`SLOT_MAX`], [16. Maximum number of data regions],
  [`UID`], [A non-zero integer. 0 means "unused"],
  [`Type`], [`{BYTE, I16, I32, I64, ARR_*, STATIC_ARR_*, DYN_ALLOC}` (`*` is the element width 1/2/4/8)],
  [`Meta`], [A partial function from `UID` to `(type, size, offset)`, with a domain of at most `SLOT_MAX` elements],
  [`Region`], [A half-open interval of physical addresses `[base, base + size)`],
)

= 3. State Variables

== 3.1 State held by the SM (inaccessible to the host)
#table(
  columns: (5fr, 4fr, 11fr),
  table.header([*Variable*], [*Type*], [*Meaning*]),
  [`state[e]`], [`State`], [Lifecycle state of the enclave],
  [`share[e]`], [`Share`], [Sharing state: the permission granted (or to be granted) to the host],
  [`meta[e]`], [`Meta`], [Expected metadata used by the next validation],
  [`phase[e]`], [`Phase`], [Internal progress of run / resume],
  [`runner[e]`], [`Harts ∪ {⊥}`], [The hart executing e, or processing run / resume for e],
  [`ctx[h]`], [`HOST`, `ENC(e)`, `SM(e)`], [Context of hart h. `SM(e)` means it is processing an SBI call for e],
  [`epm[e]`, `utm[e]`, `adm[e]`], [`Region ∪ {⊥}`], [Placement of each region],
)

== 3.2 Hardware state
#table(
  columns: (2fr, 5fr, 7fr),
  table.header([*Variable*], [*Type*], [*Meaning*]),
  [`pmp[h][e]`], [`Perm`], [S/U-mode permission on e's ADM at hart h],
  [`mem[e]`], [ADM contents (byte sequence)], [Contents of e's ADM],
)

== 3.3 Ghost variables (auxiliary, for stating properties; absent from the implementation)
#table(
  columns: (2fr, 12fr),
  table.header([*Variable*], [*Meaning*]),
  [`secret[e]`], [`mem[e]` holds contents the enclave keeps confidential (not disclosed by its own intent). It becomes true on an enclave write, and false on a stop with RONLY / RW (intended disclosure), on an adversary write, and on zeroing],
  [`dirty[e]`], [Someone other than the owner enclave may have written since the last successful validation. It becomes true on Create, on a stop with RW, and on an adversary write, and returns to false only when the contents were actually checked and passed],
  [`sentMeta[e]`], [The metadata most recently supplied by a legitimate source: the argument of create, or the argument of the owner enclave's stop(RW)],
  [`snap[e]`], [`mem[e]` at the most recent stop],
  [`lastShare[e]`], [The sharing state specified at the most recent stop],
  [`dead[e]`], [Validation has failed (this incarnation of the enclave must never execute again)],
  [`observed`], [The set of `(e, contents)` pairs read by the adversary],
)

= 4. Initial State

```
∀e: state[e] = INVALID, share[e] = NONE, meta[e] = ∅, phase[e] = IDLE,
    runner[e] = ⊥, epm[e] = utm[e] = adm[e] = ⊥,
    dirty[e] = FALSE, dead[e] = FALSE
∀h: ctx[h] = HOST
∀h, e: pmp[h][e] = NONE
observed = ∅
```

= 5. Transitions

Each transition means "when the guard holds, the effect happens atomically". Variables not mentioned are unchanged.

== 5.1 SM operations

=== Create(h, e, R_epm, R_utm, R_adm, m0) — issued by the host
#table(
  columns: (1fr, 6fr),
  [Guard], [`ctx[h] = HOST`, `state[e] = INVALID`, `NoOverlap(R_epm, R_adm)` (C1 in Section 7.3), a PMP entry is available, `|dom(m0)| ≤ SLOT_MAX`],
  [Effect], [`epm[e]' = R_epm`, `utm[e]' = R_utm`, `adm[e]' = R_adm`, `∀h': pmp[h'][e]' = NONE`, `share[e]' = RW`, `meta[e]' = m0`, `state[e]' = FRESH`, `dirty[e]' = TRUE`, `sentMeta[e]' = m0`, `dead[e]' = FALSE`],
)

- `share = RW` expresses "the host has initialized the ADM, so validation is required before the first run".
- `m0` is supplied by the host (the adversary). The initial validation guarantees only that the ADM is consistent with `m0`. Whether the initial state (`m0` and the initial contents) is correct is a matter of judging, through attestation, whether the enclave was launched correctly, and is outside this model (dissertation Section 3.6.4).
- If the guard does not hold, nothing changes (C5).

=== RunBegin(h, e) — issued by the host
#table(
  columns: (1fr, 6fr),
  [Guard], [`ctx[h] = HOST`, `state[e] = FRESH`],
  [Effect], [`state[e]' = RUNNING`, `ctx[h]' = SM(e)`, `runner[e]' = h`, `phase[e]' = VALIDATING`],
)

Since every hart already has permission NONE after create, the transition proceeds to validation without a revocation phase.

=== ResumeBegin(h, e) — issued by the host
#table(
  columns: (1fr, 6fr),
  [Guard], [`ctx[h] = HOST`, `state[e] = STOPPED`, `phase[e] = IDLE`],
  [Effect], [`state[e]' = RUNNING`, `ctx[h]' = SM(e)`, `runner[e]' = h`, `phase[e]' = REVOKING`],
)

=== RevokeStep(h', e) — SM internal (application on each hart via IPI)
#table(
  columns: (1fr, 6fr),
  [Guard], [`phase[e] = REVOKING`, `pmp[h'][e] ≠ NONE`],
  [Effect], [`pmp[h'][e]' = NONE`],
)

=== RevokeDone(e) — SM internal
#table(
  columns: (1fr, 6fr),
  [Guard], [`phase[e] = REVOKING`, `∀h': pmp[h'][e] = NONE`],
  [Effect], [`phase[e]' = VALIDATING`],
)

Revocation is split into per-hart steps so that the properties can state that neither validation nor enclave resumption begins in an intermediate state of IPI synchronization (where only some harts still hold the permission).

=== ValidatePass(e) — SM internal
#table(
  columns: (1fr, 6fr),
  [Guard], [`phase[e] = VALIDATING`, and either `share[e] ≠ RW` or `WF(mem[e], |adm[e]|, meta[e])` (Section 6)],
  [Effect], [`share[e]' = NONE`, `meta[e]' = ∅`, `dirty[e]' = FALSE` only if `share[e] = RW` (the contents were checked), followed by Enter],
)

*Enter* (part of the effect of ValidatePass): with `h = runner[e]`, `pmp[h][e]' = RW`, `ctx[h]' = ENC(e)`, `phase[e]' = IDLE`.

=== ValidateFail(e) — SM internal
#table(
  columns: (1fr, 6fr),
  [Guard], [`phase[e] = VALIDATING`, `share[e] = RW`, `¬WF(mem[e], |adm[e]|, meta[e])`],
  [Effect], [`dead[e]' = TRUE`, `ctx[runner[e]]' = HOST`, `runner[e]' = ⊥`, `phase[e]' = IDLE`, followed by DestroyBody(e)],
)

This corresponds to "the SM immediately destroys the enclave" in dissertation Section 3.6.4.

=== StopShare(h, e, s, m) — issued by the owner enclave (edge call)
#table(
  columns: (1fr, 6fr),
  [Guard], [`ctx[h] = ENC(e)`, `state[e] = RUNNING`, `s ∈ Share`, `s = RW ⇒ m ≠ ⊥ ∧ |dom(m)| ≤ SLOT_MAX`],
  [Effect (common)], [`state[e]' = STOPPED`, `share[e]' = s`, `ctx[h]' = HOST`, `runner[e]' = ⊥`, `snap[e]' = mem[e]`, `lastShare[e]' = s`],
  [Effect (`s = NONE`)], [`pmp[h][e]' = NONE`, `meta[e]' = ∅`],
  [Effect (`s = RONLY`)], [`∀h': pmp[h'][e]' = R`, `meta[e]' = ∅`],
  [Effect (`s = RW`)], [`∀h': pmp[h'][e]' = RW`, `meta[e]' = m`, `sentMeta[e]' = m`, `dirty[e]' = TRUE`],
)

- A call whose guard does not hold (`s` out of range, or `RW` without metadata) is rejected and changes no state.
- Granting is modeled as atomic. In the intermediate states of granting, the calling hart is inside the SM and the enclave is not executing, so splitting it into steps does not affect the properties.
- Granting targets every hart, including harts executing other enclaves, because sharing is disclosure to the untrusted side (A6 is a property of the non-shared state only).

=== StopInvoluntary(h, e) — a stop without a sharing request, such as a timer interrupt
#table(
  columns: (1fr, 6fr),
  [Guard], [`ctx[h] = ENC(e)`, `state[e] = RUNNING`],
  [Effect], [Same as StopShare(h, e, NONE, ⊥)],
)

=== Exit(h, e) — issued by the owner enclave
#table(
  columns: (1fr, 6fr),
  [Guard], [`ctx[h] = ENC(e)`, `state[e] = RUNNING`],
  [Effect], [Same as StopShare(h, e, NONE, ⊥)],
)

=== Destroy(h, e) — issued by the host
#table(
  columns: (1fr, 6fr),
  [Guard], [`ctx[h] = HOST`, `state[e] ∈ {FRESH, STOPPED}`, `phase[e] = IDLE`],
  [Effect], [`state[e]' = DESTROYING`, followed by DestroyBody(e)],
)

*DestroyBody(e)* (executed in this order):

+ `mem[e]' = 0` (zero the ADM and the EPM)
+ Free the entries `pmp[h'][e]` for all `h'`. `adm[e]' = epm[e]' = utm[e]' = ⊥` (the physical region belongs to the host from here on)
+ `share[e]' = NONE`, `meta[e]' = ∅`, `state[e]' = INVALID`

== 5.2 Owner enclave operations
#table(
  columns: (3fr, 7fr, 4fr),
  table.header([*Transition*], [*Guard*], [*Effect*]),
  [EncRead(h, e)], [`ctx[h] = ENC(e)`, `pmp[h][e] ∈ {R, RW}`], [The enclave reads `mem[e]`],
  [EncWrite(h, e, v)], [`ctx[h] = ENC(e)`, `pmp[h][e] = RW`], [`mem[e]' = v`],
)

== 5.3 Adversary operations
Hart h is considered adversarial with respect to e when `ctx[h] = HOST`, or `ctx[h] = ENC(e2)` with `e2 ≠ e`.

#table(
  columns: (3fr, 8fr, 12fr),
  table.header([*Transition*], [*Guard*], [*Effect*]),
  [AdvRead(h, e)], [h is adversarial w.r.t. e, `pmp[h][e] ∈ {R, RW}`], [`observed' = observed ∪ {(e, mem[e])}`],
  [AdvWrite(h, e, v)], [h is adversarial w.r.t. e, `pmp[h][e] = RW`], [`mem[e]' = v` (any value), `dirty[e]' = TRUE`, `secret[e]' = FALSE`],
  [Host SBI], [`ctx[h] = HOST`], [Attempt Create / RunBegin / ResumeBegin / Destroy of Section 5.1 with any arguments],
  [Malicious enclave], [`ctx[h] = ENC(e2)`], [Attempt StopShare / Exit for e2 with any arguments],
)

= 6. Specification of the Validation Function

The dissertation treats the validation function as an example that can be extended for each use. WF in this section specifies the structural check of the current implementation. It does not include checks based on the meaning of the data, which the dissertation leaves as future work. For this reason the checks of the validation function (B6, B7) stay bounded, and a proof that does not depend on the bound is not a goal.

== 6.1 ADM data structure (dissertation Section 3.6.3)
Positions are offsets from the base of the ADM. Let `S` be the size of the ADM.

```
ADM header (offset 0, size HDR)
  n            : number of active data regions
  free         : offset where the next region header goes (free_list)
  hoff[0..15]  : offset to each region header
  uid[0..15]   : UID of each region

Region header (offset hoff[i], size RHDR)
  type         : type of the region
  doff         : offset to the data body
  dsize        : size of the data body (bytes)
```

For region i, write `H_i = [hoff[i], hoff[i] + RHDR)` (header) and `D_i = [doff_i, doff_i + dsize_i)` (data body). All arithmetic is over mathematical integers (no wrap-around on overflow).

== 6.2 Structural predicate WF(mem, S, meta)
All of the following hold.

#table(
  columns: (2fr, 12fr),
  table.header([*ID*], [*Condition*]),
  [W1], [`n ≤ SLOT_MAX`],
  [W2], [`uid[0..n-1]` are pairwise distinct, all non-zero, and equal as a set to `dom(meta)` (the number and identity of regions match)],
  [W3], [`∀i < n`: `HDR ≤ hoff[i]` and `hoff[i] + RHDR ≤ S`],
  [W4], [`∀i < n`: `doff_i + dsize_i ≤ S`],
  [W5], [`hoff[0] < hoff[1] < … < hoff[n-1]`],
  [W6], [The ADM header `[0, HDR)`, every `H_i`, and every `D_i` are pairwise disjoint],
  [W7], [`∀i < n`: `type_i = meta[uid[i]].type`],
  [W8], [`∀i < n`: `doff_i = meta[uid[i]].offset`],
  [W9], [`∀i < n`: `SizeOK(type_i, dsize_i, meta[uid[i]].size)`],
  [W10], [`∀i < n`: `doff_i = hoff[i] + RHDR` (the data body follows its region header)],
  [W11], [`HDR ≤ free`, and `∀i < n`: `doff_i + dsize_i ≤ free`],
  [W12], [`free` and every `hoff[i]` (`i < n`) are multiples of 8],
)

`SizeOK(t, d, s)`:
#table(
  columns: (5fr, 8fr),
  table.header([*Type*], [*Condition*]),
  [BYTE / I16 / I32 / I64], [`d = 1 / 2 / 4 / 8`],
  [STATIC_ARR_w (w = 1, 2, 4, 8)], [`d = s × w`],
  [ARR_w (variable-length array)], [No constraint (only the range checks W4 and W6)],
  [DYN_ALLOC], [`d = s`],
)

A variable-length array is the type an enclave selects when it does not care about the size of the data; not comparing `dsize` with an expected value is intended by design. For this type, only the number, type, offset, and range (W4, W6) are validated.

By W5, W6 and W10, the regions are in the order of their index (`doff_i + dsize_i ≤ hoff[i+1]`). By W8 and W10, `meta` also fixes the position of the region headers. W10–W12 are the layout that the enclave-side library relies on when it adds and removes regions (Section 8 \#16). There is no upper bound on `free`: when the ADM is almost full, the library sets `free` above S.

== 6.3 Requirements on the validation function

- *Soundness (B6)*: if the validation function returns success, then `WF(mem, S, meta)` holds.
- *Safety (B7)*: for any `mem`, any `meta`, and any `S > 0`, the validation function reads nothing outside `[0, S)`, writes nothing, causes no integer overflow, and terminates.

= 7. Properties

== 7.1 A. Access control (R1)

#table(
  columns: (2fr, 12fr, 12fr, 2fr),
  table.header([*ID*], [*Claim*], [*Formal statement*], [*Dissertation*]),
  [A1], [Right after creation, no hart can access the ADM], [`state[e] = FRESH ⇒ ∀h: pmp[h][e] = NONE`], [3.6.2],
  [A2], [While running, only the executing hart can access the ADM], [`ctx[h] = ENC(e) ⇒ pmp[h][e] = RW ∧ ∀h2 ≠ h: pmp[h2][e] = NONE`], [3.6.2, 3.8.1],
  [A3], [The permission of anyone but the owner never exceeds the sharing state the enclave specified], [`ctx[h] ≠ ENC(e) ∧ adm[e] ≠ ⊥ ⇒ pmp[h][e] ≤ hostPerm(share[e])`], [3.5.2, 3.8.1],
  [A4], [The host cannot resume the enclave while retaining access], [Enter occurs only when `∀h: pmp[h][e] = NONE`], [3.6.5, 3.8.1],
  [A5], [The ADM never has execute permission], [The ADM's PMP entry has X = 0 on every hart at all times], [3.9.3],
  [A6], [When not shared, other enclaves cannot access the ADM], [`share[e] = NONE ∧ pmp[h][e] ≠ NONE ⇒ ctx[h] = ENC(e)`], [3.6.5],
  [A7], [The host cannot change the sharing state or the metadata], [The only transitions that change `share[e]` or `meta[e]` are Create(e), StopShare / StopInvoluntary / Exit (issued by e), ValidatePass(e), and DestroyBody(e)], [3.6.4],
  [A8], [An involuntary stop grants nothing to the host], [After StopInvoluntary, `share[e] = NONE ∧ ∀h: pmp[h][e] = NONE`], [3.8.1],
  [A9], [On destruction, what the enclave keeps confidential is erased before the region returns to the host], [`¬secret[e]` immediately before the transition in which `adm[e]` becomes `⊥`], [—],
)

A3 implies A6 when `share[e] = NONE`, and implies the second half of A2 while running (where `share = NONE` by D2).

A9 reduces to "the ADM contents are never disclosed without the enclave's intent" (E1). Releasing the region discloses it to the host, so A9 is the case of E1 at the moment of release. A9 is not stated as "`mem[e] = 0` at release": if the enclave is destroyed while shared as RW, the host can write between zeroing and release, so that form does not hold (a counterexample was confirmed with TLC). What remains in that case was written by the host itself, not something the enclave keeps confidential.

== 7.2 B. Enforcement of validation (R3)

#table(
  columns: (2fr, 12fr, 12fr, 2fr),
  table.header([*ID*], [*Claim*], [*Formal statement*], [*Dissertation*]),
  [B1], [After the host could write, the enclave resumes only after validation], [`ctx[h] = ENC(e) ⇒ ¬dirty[e]`], [3.6.4, 3.8.2],
  [B2], [From the start of validation until resumption, the host cannot write], [`phase[e] = VALIDATING ⇒ ∀h: pmp[h][e] = NONE`], [3.5.2, 3.8.2],
  [B3], [An enclave that failed validation never executes again], [`dead[e] ⇒ state[e] ∈ {DESTROYING, INVALID} ∧ ∀h: ctx[h] ≠ ENC(e)` (until Create is issued again for the same ID)], [3.6.4],
  [B4], [Validation uses the metadata most recently supplied by a legitimate source], [`phase[e] = VALIDATING ∧ share[e] = RW ⇒ meta[e] = sentMeta[e]`], [3.6.4],
  [B5], [The first run always goes through validation], [Every path from FRESH to `ctx[h] = ENC(e)` passes through ValidatePass (follows from B1 and `dirty' = TRUE` in Create)], [3.6.4],
  [B6], [The validation function is sound], [Section 6.3], [3.6.4],
  [B7], [The validation function is safe], [Section 6.3], [3.8.3],
  [B8], [During validation, only the SM can access the ADM], [Same formula as B2 (no hart has S/U-mode permission)], [3.6.5],
)

B2 expresses the order "revoke → validate → grant" as an invariant.

== 7.3 C. PMP configuration (R2)

#table(
  columns: (2fr, 9fr, 12fr, 2fr),
  table.header([*ID*], [*Claim*], [*Formal statement*], [*Dissertation*]),
  [C1], [The ADM does not overlap other protected regions], [`NoOverlap`: `adm[e]` is disjoint from the SM region, `epm[e]`, `utm[e]`, and `epm[e2]` and `adm[e2]` for `e2 ≠ e`], [3.8.3],
  [C2], [Priority order of entries], [SM entry < EPM and ADM entries of each enclave < UTM / host OS entry (a lower number means higher priority)], [3.6.1],
  [C3], [The effective protected range of the ADM is as specified], [The set of addresses matched by the ADM's entry = `adm[e]`. Rewriting the address register used as the TOR lower bound does not change the matching range of any other entry], [3.6.1, 3.9.2],
  [C4], [Independence from the NAPOT condition], [The guard of Create can be satisfied for any page-aligned `base` and any `size > 0` that is a multiple of the page size (as long as an entry is available)], [3.6.1],
  [C5], [No intermediate state is left on failure], [When Create fails, every variable equals its value before the call], [—],
)

C1 matters because the ADM's entry may have higher priority than the entry of another enclave's private memory. If overlap were allowed, a malicious enclave could expose another enclave's private memory by sharing its own ADM.

Two consequences of C1 are checked. Both are stated over the permissions of every enclave entry that matches an address, so they hold regardless of the priority order among those entries.

- *Isolation of private memory*: for each page of e's private memory, on a hart that is not executing e, the only matching entry is e's EPM entry (no permission). Sharing an ADM never exposes another enclave's private memory.
- *Faithfulness of the ADM permission*: for each page of e's ADM, the permission of the matching entries is exactly `pmp[h][e]`. This lifts the properties of groups A and B from the abstract `pmp[h][e]` to physical addresses.

== 7.4 D. Lifecycle and concurrency

#table(
  columns: (2fr, 6fr, 12fr),
  table.header([*ID*], [*Claim*], [*Formal statement*]),
  [D1], [Restriction on SBI callers], [StopShare / StopInvoluntary / Exit are accepted only from a hart with `ctx[h] = ENC(e)`, and only for itself. Create / RunBegin / ResumeBegin / Destroy are accepted only from `ctx[h] = HOST`],
  [D2], [State consistency], [(a) `ctx[h] = ENC(e) ⇒ state[e] = RUNNING ∧ phase[e] = IDLE ∧ share[e] = NONE ∧ meta[e] = ∅` (b) `share[e] = RW ∧ state[e] ≠ INVALID ⇒ meta[e] = sentMeta[e]` (c) `state[e] = INVALID ⇒ adm[e] = ⊥ ∧ share[e] = NONE ∧ meta[e] = ∅` (d) for each e, at most one h has `ctx[h] ∈ {ENC(e), SM(e)}`],
  [D3], [Robustness against concurrent SBI calls], [The invariants of Sections 7.1–7.3 hold under any interleaving of the transitions in Section 5 (including simultaneous calls from several harts)],
  [D4], [Completeness of IPI synchronization], [The guard of RevokeDone, `∀h: pmp[h][e] = NONE`, holds at the point where the revocation request returns in the implementation (the implementation-side proof of H3 and H4)],
)

== 7.5 E. Top-level security properties

#table(
  columns: (3fr, 12fr, 12fr, 3fr),
  table.header([*ID*], [*Claim*], [*Formal statement*], [*Derived from*]),
  [E1 Confidentiality], [The only ADM contents the adversary can observe are those of intervals the enclave shared, those the host itself placed between Create and the first run, and what remains after destruction (zeros, or what the host itself wrote)], [`(e, v) ∈ observed ⇒ share[e] ∈ {RONLY, RW}` at the time v was read. As state invariants: `secret[e] ∧` h is adversarial w.r.t. e `⇒ pmp[h][e] = NONE`, together with A9], [A2, A3, A6, A9],
  [E2 Integrity], [The ADM contents the enclave reads are either what it left or what passed validation], [At Enter: if `lastShare[e] ∈ {NONE, RONLY}` then `mem[e] = snap[e]`; if `lastShare[e] = RW` (or on the first run) then `WF(mem[e], |adm[e]|, sentMeta[e])`], [A3, B1, B2, B4, B6],
)

The second half of E2 guarantees only the structure (number, types, sizes, offsets), not the correctness of the values in the data bodies (dissertation Section 3.8.2).

= 8. Differences between the Dissertation and the Implementation

These are candidates found by code reading. For \#1, \#5, \#6, \#7, \#11, \#12 and \#13, the effect on the properties of Section 7 was checked with the implementation model of Section 10.3. The others are outside the scope of the model or are handled at the code level.

\#1–\#7 and \#11–\#16 have been fixed (in `sm/src/`, except the library and the runtime of \#16). For \#16, W10–W12 are added to WF and checked by the SM (`adm.c`), and the library and the runtime are corrected (`sdk/src/edge/adm.c`, `runtime/call/syscall.c`). Removing a region no longer moves the other regions. For \#15, both buffers are checked to be inside the EPM of the calling enclave, and the identifier is limited to 1024 bytes (`enclave.c`). \#14 is fixed in `pmp.c` after the CBMC check of Section 10.4. \#2–\#4 are fixed in `adm.c` after the CBMC check of Section 10.4. \#1 and \#5 are fixed as `FixValidateFail` and `FixStaleMeta` of Section 10.3 (`enclave.c`). For \#6, \#11 and \#12, `stop_enclave_with_share` checks its arguments and the state first, and the guard of `destroy_enclave` is limited to FRESH / STOPPED. \#7 and \#13 are fixed as `FixHartRestart` (`init_resend`), `FixNonRetSuspend` and `FixStopping` of Section 10.3 (`pmp.c`, `ipi.c`, `sm.c`, `enclave.c`). The re-send of \#7 was corrected after its first version turned out to leave a race (Section 10.3). The "Implementation" column of the table describes the code before the fix.

#table(
  columns: (1fr, 3fr, 6fr, 8fr, 7fr),
  table.header([*\#*], [*Related properties*], [*Design model*], [*Implementation*], [*Location*]),
  [1], [B3, A9], [The enclave is destroyed on validation failure], [Only an error is returned. The enclave stays at `state = RUNNING` with `n_thread = 1`. In this state neither resume (`n_thread < MAX_ENCL_THREADS` is false) nor destroy (`state <= STOPPED` is false) is accepted, so the enclave does not execute, but zeroing and PMP entry release do not happen either], [`enclave.c` `run_enclave`, `resume_enclave`],
  [2], [B6 (W2)], [The number of regions and the UID set match `meta`], [Only the `data_n` regions on the ADM side are traversed. Validation succeeds even if a region in `meta` is missing from the ADM. Duplicate UIDs are not checked], [`adm.c` `validate_adm_regions`],
  [3], [B6 (W7)], [The `type` in the region header is compared with `meta`], [The `type` field of the region header is never read. The size check uses the type from `meta`], [same],
  [4], [B6 (W3, W4, W6), B7], [Overflow-free range checks; headers and data bodies are disjoint], [`data->offset + data->size` has no overflow check. Layouts where a data body overlaps the ADM header or a region header are not excluded. The last region has no overlap check against a successor. CBMC also found region headers overlapping each other or the ADM header, and overflow of the `meta` size times the element width (Section 10.4)], [same],
  [5], [B4, D2(a)], [`meta` is replaced on every stop and cleared unless `RW`], [Only the first `count` entries are overwritten; the rest keep their previous values. Metadata passed with `NONE` / `RONLY` is stored and is not cleared on resume], [`enclave.c` `stop_enclave_with_share`],
  [6], [A7, D2], [Out-of-range `s` is rejected], [`share_type` is stored without a check (under `DEFAULT_NOPERM` it is later corrected to `NONE`). `share` is updated even when `state ≠ RUNNING`, before the failure is returned], [same, `context_switch_to_host`],
  [7], [D4 (H4)], [Every hart is a synchronization target], [The IPI targets the set of harts that are interruptible (STARTED or SUSPENDED) at that moment. In addition, a hart restarted with HSM hart_stop / hart_start goes through the OpenSBI warm boot, `sm_init(false)` and `pmp_init()`, and its PMP is initialized with the SM and OS regions only. The entries of existing enclaves are not programmed. A hart woken from non-retentive suspend goes through `init_warm_resume`, where `sbi_hart_pmp_configure` programs the root domain's "all memory" region with a higher priority than every SM entry; `final_init` is not called], [`ipi.c` `send_and_sync_pmp_ipi`, `sm.c` `sm_init`, OpenSBI 1.1 `sbi_hsm_hart_interruptible_mask`, `init_warm_startup`, `init_warm_resume`],
  [8], [6.1], [The ADM header holds the ADM size], [The SM-side `AdmHeader` has no size field and has `free_list` instead. The size is taken from the PMP region information], [`adm.h`],
  [9], [Scope], [Fixed to `DEFAULT_NOPERM`], [`DEFAULT_READ` can be selected through `create_args.adm_protection`. Implementation verification takes `DEFAULT_NOPERM` as a precondition], [`enclave.c` `create_enclave`],
  [10], [C4], [—], [The `base` and `size` of a region are restricted to page (4 KiB) granularity, coarser than the 4-byte granularity of TOR itself], [`pmp.c` `pmp_region_init`],
  [11], [D3, A9], [The guard of Destroy is `state ∈ {FRESH, STOPPED}`], [The guard is `state >= 0 ∧ state <= STOPPED`, which also admits `DESTROYING` (= 0) and `ALLOCATED`. A destroy for the same ID from another hart may be accepted while destruction is in progress (inherited from the original Keystone)], [`enclave.c` `destroy_enclave`, `ENCLAVE_EXISTS`],
  [12], [A2, A9, D2, E1], [Out-of-range arguments are rejected without changing the state], [The check for `type_info == NULL` with `share_type == RW` comes after `n_thread` has been decremented and `state` set to `STOPPED`. In that case an error is returned without switching to the host, so the enclave keeps running while the SM records `STOPPED` and `n_thread = 0` (found by code reading while building the implementation model)], [`enclave.c` `stop_enclave_with_share`],
  [13], [A1–A4, A6, A8, B1, B2, E1, E2], [A stop is atomic: when the enclave is STOPPED, the PMP switch is complete], [`stop_enclave_with_share`, `stop_enclave` and `exit_enclave` set `state = STOPPED` under the lock, drop the lock, and only then call `context_switch_to_host`. That function reads `share` as it is at that moment, calls `pmp_set_global` / `pmp_set_keystone`, and swaps the register context in `threads[0]`. Between dropping the lock and the end of the switch, a resume or destroy from another hart is accepted (found by checking the implementation model; switching outside the lock is inherited from the original Keystone)], [`enclave.c` `stop_enclave_with_share`, `stop_enclave`, `exit_enclave`, `context_switch_to_host`],
  [14], [C2, C3], [The entries of the ADM and the EPM lie between the SM and OS entries and are shared with no other region], [A UTM (`PMP_PRI_BOTTOM`) shares the lowest-priority register with the OS region. Freeing a UTM clears that bit of `reg_bitmap`, so once `PMP_PRI_ANY` allocations (EPM, ADM) have used up registers 1–6, the next one receives the lowest-priority register. The entry of that region is overwritten by the OS region (all memory, all permissions) on every switch to the host. The host only needs to create and destroy enclaves (found with CBMC; inherited from the original Keystone)], [`pmp.c` `get_free_reg_idx`, `get_conseq_free_reg_idx`, `pmp_region_free_atomic`],
  [15], [H8 (premise of E1, E2)], [The SM reads and writes only memory the caller can access], [`get_sealing_key` uses the two physical addresses passed by the enclave (where the key is written, where the identifier is read) in M-mode as they are, without a range check. The identifier length is unbounded as well, and is the size of a variable-length array on the SM stack. A malicious enclave can overwrite any physical address with the key and its signature (192 bytes). By having one byte read as the identifier and comparing the derived key with the keys of its own 256 candidates, it can also read any physical address (the EPM and ADM of other enclaves, the private key of the SM). Found by code reading; inherited from the original Keystone. The write outside the EPM was confirmed on QEMU with the UTM as the destination (Section 10.5); the read was not run], [`enclave.c` `get_sealing_key`, `sm.c` `sm_derive_sealing_key`],
  [16], [E2 (B6)], [An ADM that passes validation is safe to use with the enclave-side library], [Validation (W1–W9) constrains neither the position of a region header relative to its data, nor the order of the regions, nor `free_list`. The library trusts all three. If the host rewrites them during a writable share, the ADM still passes validation, and adding or removing a region after the resume writes outside the ADM (the size computations of `adm_setup_new_hdr` and `adm_remove_region` wrap around). The runtime removes and adds a region for every delegated syscall. The library itself was also wrong: `adm_remove_region` does not update `offset` in the headers of the later regions when it removes a region other than the last, so that without any action by the host the data is misplaced and the next validation fails; `adm_reset` writes an address, not an offset, to `free_list` (found by code reading; the removal error was confirmed with a unit test and on QEMU). `adm_gen_type_info` and the runtime put the size in bytes, not the number of elements, into the metadata of a static-length array (found by the CBMC check)], [`sdk/src/edge/adm.c`, `runtime/call/syscall.c`, `sm/src/adm.c` `validate_adm_regions`],
)

= 9. Correspondence between the Model and the Implementation

#table(
  columns: (11fr, 12fr, 6fr),
  table.header([*Model transition*], [*Implementation function*], [*File*]),
  [Create], [`create_enclave`, `is_create_args_valid`], [`sm/src/enclave.c`],
  [RunBegin, ValidatePass / ValidateFail (first run)], [`run_enclave`], [`sm/src/enclave.c`],
  [ResumeBegin, RevokeStep, RevokeDone, ValidatePass / ValidateFail], [`resume_enclave` (revocation is `pmp_set_global(rid, PMP_NO_PERM)`)], [`sm/src/enclave.c`],
  [Enter], [`context_switch_to_enclave`], [`sm/src/enclave.c`],
  [StopShare], [`sbi_sm_stop_enclave_with_share` → `stop_enclave_with_share` → `context_switch_to_host`], [`sm/src/sm-sbi.c`, `sm/src/enclave.c`],
  [StopInvoluntary], [`sbi_sm_stop_enclave` → `stop_enclave`], [same],
  [Exit], [`sbi_sm_exit_enclave` → `exit_enclave`], [same],
  [Destroy, DestroyBody], [`destroy_enclave`], [`sm/src/enclave.c`],
  [Validation function (Section 6)], [`validate_adm_regions`], [`sm/src/adm.c`],
  [Metadata intake], [`copy_adm_type_info`], [`sm/src/adm_utils.h`],
  [Adding and removing regions and generating the metadata on the enclave side (WF is preserved; Section 10.4)], [`adm_new_region`, `adm_remove_region`, `adm_gen_type_info`], [`sdk/src/edge/adm.c`],
  [Single-hart update of `pmp[h][e]`], [`pmp_set_keystone`], [`sm/src/pmp.c`],
  [All-hart update of `pmp[h][e]`], [`pmp_set_global` → `send_and_sync_pmp_ipi`], [`sm/src/pmp.c`, `sm/src/ipi.c`],
  [Region overlap check (C1)], [`detect_region_overlap`, `pmp_region_init`], [`sm/src/pmp.c`],
  [TOR / NAPOT selection and placement (C2–C4)], [`pmp_region_init`, `tor_region_init`, `napot_region_init`], [`sm/src/pmp.c`],
  [Caller restriction (D1)], [`sbi_ecall_keystone_enclave_handler` (`cpu_is_enclave_context`)], [`sm/src/sm-sbi-opensbi.c`],
  [`state[e]`], [`enclaves[eid].state` (the implementation's `ALLOCATED` is an intermediate state between ID allocation and completion of create; it does not appear in the design model, where Create is atomic; the implementation model has it)], [`sm/src/enclave.h`],
  [`share[e]`, `meta[e]`], [`enclaves[eid].adm_state_info`], [`sm/src/enclave.h`, `sm/src/adm.h`],
)

= 10. Verification Status

== 10.1 Model checking of the design model with TLA+

The specifications are in `verif/tla/`. `make check` runs the checks and `make reach` confirms reachability. The guarantee extends only to the size of the checked configurations.

#table(
  columns: (3fr, 4fr, 9fr),
  table.header([*Property*], [*Specification / configuration*], [*Result and treatment*]),
  [A1–A4, A6–A9], [`ADM.tla` / `ADM.cfg` (3 harts, 2 enclaves)], [No counterexample],
  [A5], [same], [Holds by construction: the range of permissions has no X],
  [B1–B5, B8], [same], [No counterexample],
  [B6, B7], [—], [Properties of the validation function itself; checked with CBMC in Section 10.4],
  [C1, C2, and the consequences of C1], [`ADMRegions.tla` / `ADMRegions.cfg` (2 harts, 2 enclaves, 5 pages)], [No counterexample. The UTM is not placed (it lies under the lowest-priority entry, so the ADM entry wins where they overlap). The overlap check of the implementation is checked in Section 10.4],
  [C3], [—], [The TOR / NAPOT encoding is an implementation matter. The model assumes that an entry matches exactly its own region; the implementation is checked with CBMC in Section 10.4],
  [C4], [`ADMRegions.tla`], [Confirmed that an enclave whose ADM is not a power of two in size, or not aligned, reaches the running state],
  [C5], [—], [Holds by construction because transitions are atomic; the implementation is checked in Section 10.4],
  [D1, D2], [`ADM.tla` / `ADM.cfg`], [No counterexample],
  [D3], [same], [TLC explores every interleaving, so this is included in the results above; each transition of Section 5 is treated as atomic],
  [D4], [same], [An assumption in the design model (the guard of RevokeDone). Confirmed that B2 is violated when the guard is removed],
  [E1, E2], [same], [No counterexample, in the form of state invariants],
)

That the checks are not vacuous is confirmed in two ways.

- *Reachability*: all of the following are reached: resumption after a writable share passes validation, two enclaves running at the same time, a shared ADM writable from a hart running another enclave, an intermediate state of revocation, a state where validation fails, and execution with a non-NAPOT ADM.
- *Mutation tests*: violations are detected for each of the following: skipping the revocation phase (A2), not waiting for every hart to revoke (B2), not checking the contents (B5), and omitting the overlap check against other enclaves (C1 and the isolation of private memory).

== 10.2 Proofs of the design model in Rocq

The proofs are in `verif/rocq/`. `make prove` checks every proof and confirms that no theorem depends on an axiom (every `Print Assumptions` reports "Closed under the global context").

The difference from TLA+ is the scope of the guarantee. Harts, enclaves, ADM contents and metadata are arbitrary types (the only assumption is that equality of harts and of enclave IDs is decidable), and the structural predicate WF is kept as an arbitrary predicate. The TLA+ model fixed WF(d, m) to d = m. The results below therefore do not depend on the size of the configuration.

The method is as follows. The transitions of `ADM.tla` are transcribed as an inductive transition relation `step`, and an inductive invariant `Inv` is shown to hold initially and to be preserved by every transition. The properties of Section 7 are derived from `Inv`. The step properties (A4, A7, A9, the step form of B5, and D1) are stated as theorems about one step from a reachable state. Appendix A gives the definitions and theorems as formulas.

#table(
  columns: (3fr, 3fr, 10fr),
  table.header([*Property*], [*File*], [*Result and treatment*]),
  [A1–A4, A6–A9], [`ADM.v`], [Proved],
  [A5], [—], [Holds by construction: the range of permissions has no X],
  [B1–B5, B8], [`ADM.v`], [Proved],
  [C1, C2, and the consequences of C1], [`ADMRegions.v`], [Proved. Addresses are an arbitrary type and regions arbitrary non-empty sets; contiguity and alignment are not used],
  [D1, D2], [`ADM.v`], [Proved. D2(d) is stated in a form independent of the number of harts: at most one hart executes or enters a given enclave],
  [D3], [`ADM.v`], [The transition relation admits every interleaving, so this is included in the results above. As in TLA+, each transition is atomic],
  [D4], [—], [An assumption, as in TLA+ (the guard of RevokeDone)],
  [E1, E2], [`ADM.v`], [Proved, in the form of state invariants],
)

Notes:

- *Strengthening of the invariant*: the properties of Section 7 alone are not inductive. The main additions are:
  - the correspondence between the hart executing or entering an enclave and `runner`;
  - outside RUNNING, the phase is IDLE, or ZEROED during destruction;
  - after zeroing, `secret` is false (needed for A9);
  - `dirty` implies share = RW (needed for B1);
  - unless the last stop was a writable share, share ≠ RW (needed for E2).
- *Correspondence with TLA+*: `ValidatePass` and `ValidateFail` name the runner in their guard (`runner s e = Some h`). On reachable states this agrees with `runner[e]` of TLA+.
- *Coverage of the model with regions*: `step_cases` in `ADMRegions.v` shows that every transition of the design model is a Create, a DestroyFree, or a transition that keeps the set of existing enclaves. The transitions of the model with regions therefore cover every transition of the design model.
- *Non-vacuity*: ensured by the reachability checks of TLA+ on the same model (Section 10.1).
- *Checking time*: `ADM.v` takes 4–5 minutes and about 3.5 GB of memory at peak, because a generic automation tactic case-splits every pair of transition and invariant clause.

== 10.3 Checking the implementation model

The specification is in `verif/tla/ADMImpl.tla`. It is a module independent of the design model (`ADM.tla` and the others), which is left unchanged. It follows `enclave.c`, `adm.c`, `ipi.c`, `pmp.c` and `sm.c` in `sm/src/` and OpenSBI 1.1, and splits the transitions where the code drops the lock or crosses harts.

The main points of the model are as follows.

- *PMP*: represented by the ADM entry of each hart (NONE / R / RW / unset). An unset entry falls through to the lowest-priority OS region, which grants everything to the host context and nothing to an enclave context. The properties are stated on this effective permission.
- *IPI and HSM*: `pmp_set_global` reaches only the interruptible harts. HSM hart_stop / hart_start / hart_suspend are part of the model, which drops D4, an assumption of the design model. The HSM states are refined after the warm boot and resume sequences of OpenSBI 1.1: START_PENDING is split before and after `sm_init(false)`, and the interval between STARTED and the switch to S-mode is a separate state. A hart that is not STARTED, or that is in the SM (M-mode), has no effective permission on the ADM. `pmp_set_global` is split into transitions: the record and the read of the mask, the delivery to each hart of the mask, and the re-send. The HSM states may change in between.
- *Stops*: each of the three stops is split into a transition that changes the state under the lock and a transition that performs `context_switch_to_host` outside the lock (`StopSwitch`). The latter reads `share` as it is at that moment.
- *Properties*: the properties of Section 7, with the same names and meaning, restated on the effective permission. B3 is checked in its design form (the enclave is destroyed) and in a weaker form (it never executes again).
- *Fix flags*: a `Fix*` constant per difference of Section 8. All false is the implementation before any fix; all true is the fixed version. \#7 has two flags, and `RestoreAt` selects how it is fixed (next subsection).

#table(
  columns: (3fr, 1fr, 8fr),
  table.header([*Flag*], [*Difference*], [*Fix*]),
  [`FixValidateFail`], [\#1], [Destroy an enclave that failed validation],
  [`FixStaleMeta`], [\#5], [Replace all the metadata on every stop (clear it for NULL), and clear it also on resumption from a read-only share],
  [`FixNullRW`], [\#12], [Reject a writable share with NULL metadata, and a call while not RUNNING, before changing the state],
  [`FixShareType`], [\#6], [Reject an out-of-range `share_type` before changing the state],
  [`FixDestroyGuard`], [\#11], [Accept destroy only in FRESH or STOPPED],
  [`FixHartRestart`], [\#7], [Record, per region, the value `pmp_set_global` / `pmp_unset_global` set on every hart, and program the ADM entries of a restarted hart from these records. `RestoreAt` selects where],
  [`FixNonRetSuspend`], [\#7], [The SM registers its own HSM device, which refuses non-retentive suspend],
  [`FixStopping`], [\#13], [A stop sets STOPPING under the lock, and STOPPED only after `context_switch_to_host` is done. No resume or destroy is accepted while STOPPING],
)

=== How \#7 is fixed

`RestoreAt` takes one of three values. The fixed version (`ADMImplFixed.cfg`) uses `init_resend`, which needs no change to OpenSBI.

#table(
  columns: (2fr, 9fr),
  table.header([*`RestoreAt`*], [*Meaning*]),
  [`init_resend`], [The restarted hart programs its entries from the records in `sm_init(false)` (OpenSBI's `final_init`). At that point the hart is START_PENDING, and OpenSBI restricts IPI targets to STARTED / SUSPENDED harts, so later updates do not reach it. Therefore `send_and_sync_pmp_ipi`, after the usual round, sends the update again to the harts that were outside the mask of that round, waiting while a hart is START_PENDING. An `mb()` separates writing the record from reading the hart states. No change to OpenSBI is needed],
  [`init`], [Program the entries in `sm_init(false)` only, without re-sending],
  [`started`], [Add a hook to OpenSBI and program the entries after STARTED, just before the switch to S-mode. Needs an OpenSBI patch],
)

The constant `Resend` selects the targets of the re-send. The fixed version uses `mask`.

#table(
  columns: (2fr, 9fr),
  table.header([*`Resend`*], [*Targets of the re-send*]),
  [`mask`], [Every hart that was outside the mask of the round. The sender waits until the hart is no longer START_PENDING and sends (OpenSBI drops a hart that is not interruptible)],
  [`state`], [The harts that are START_PENDING when the re-send loop looks at them. The first version of the fix did this],
  [`atomic`], [The whole `pmp_set_global` is one transition, as in the earlier model],
)

`state` has a race. A restarting hart programs its entries from the records; the sender then reads the mask; the hart becomes STARTED before the re-send loop looks at it. The hart is in neither the mask nor the re-send. OpenSBI sets STARTED after `final_init` (`sm_init`, including console output), so the window exists. `atomic` takes the read of the mask and the loop as simultaneous and does not explore this race.

A hart woken from non-retentive suspend does not go through `final_init`, so `init_resend` and `init` cannot restore its entries. OpenSBI's `hart_resume` hook is called before `sbi_hart_pmp_configure` and cannot be used either. `FixNonRetSuspend` therefore refuses non-retentive suspend altogether. The SM registers an HSM device with `sbi_hsm_set_device` in `sm_init(cold)`. Its `hart_suspend` returns an error other than `SBI_ENOTSUPP` for a non-retentive type (OpenSBI does not fall back to its default implementation) and `SBI_ENOTSUPP` for a retentive type (the default `wfi` is used). `hart_start` and `hart_stop` are not provided. If another HSM device is already registered, the SM cannot protect the ADM and stops booting. The generic, fu540 and mpfs platforms of OpenSBI 1.1 register no HSM device, and the default non-retentive suspend only executes `wfi` without powering the hart down, so refusing it costs nothing in practice.

=== Results

Unless stated otherwise, the configuration has 2 harts and 1 enclave.

- *Implementation before the fix* (`make impl-original`): every one of the 17 properties has a counterexample.
- *Current implementation* (`make impl`; every difference fixed, the same configuration as the fixed version): all 17 properties hold.
- *Before fixing \#1, \#5, \#7 and \#13* (\#6, \#11 and \#12 fixed): A1–A4, A6, A8, B1, B2, E1 and E2 have counterexamples due to \#7 and \#13, B3 in its design form due to \#1, and B4 and B5 due to \#5. Fixing \#7 alone leaves the same properties violated through \#13.
- *Fixed version* (`make check-impl`): every property holds with 3 harts and 1 enclave, and with 2 harts and 2 enclaves (with one kind of data and metadata). The six reachability witnesses (resumption after a writable share, execution while a hart is stopped, validation failure, a hart restart during a writable share, a stop in the middle of its switch, a revocation waiting to be re-sent) are reached.
- *Targets of the re-send* (`make impl-resend`; only `Resend` of the fixed version is changed): with `state`, A1–A4, A6, A8, B1, B2, E1 and E2 are violated. With `atomic`, all 17 hold and the race is not visible.
- *Ways to fix \#7* (`make impl-ablation` and `make impl-restart`; only the \#7 part of the fixed version is changed):

  #table(
    columns: (4fr, 3fr, 6fr),
    table.header([*`RestoreAt`*], [*Non-retentive suspend refused*], [*Result*]),
    [`init_resend` (no change to OpenSBI)], [yes], [All 17 hold (the fixed version)],
    [`init_resend`], [no], [A1–A4, A6, A8, B1, B2, E1, E2 violated],
    [`init` (no re-send)], [yes], [A1–A4, A6, A8, B1, B2, E1, E2 violated],
    [`started` (OpenSBI patch)], [no], [All 17 hold],
  )
- *Attribution* (`make impl-ablation`): the properties that break when exactly one fix is removed from the fixed version are as follows.

#table(
  columns: (2fr, 1fr, 8fr),
  table.header([*Fix removed*], [*Difference*], [*Properties violated*]),
  [`FixHartRestart`], [\#7], [A1, A2, A3, A4, A6, A8, B1, B2, E1, E2],
  [`FixNonRetSuspend`], [\#7], [A1, A2, A3, A4, A6, A8, B1, B2, E1, E2],
  [`FixStopping`], [\#13], [A1, A2, A3, A4, A6, A8, B1, B2, E1, E2],
  [`FixNullRW`], [\#12], [All 17],
  [`FixDestroyGuard`], [\#11], [All except B4 and B5],
  [`FixStaleMeta`], [\#5], [B4, B5],
  [`FixValidateFail`], [\#1], [B3 (design form only; the weaker form holds)],
  [`FixShareType`], [\#6], [None],
)

=== Main counterexamples

+ *Hart restart through HSM (\#7)*: when the host stops a hart with hart_stop and starts it again with hart_start, only the SM and OS regions are programmed on that hart. The host on that hart gets full access to all memory through the OS region. It can read and write the ADM even while the enclave runs on another hart, which violates A2 and E1 (presumably the EPM as well). The host alone suffices; no cooperation from the enclave is needed. Inherited from the original Keystone. Waking from non-retentive suspend has the same effect.

  Programming the entries of the restarted hart in `sm_init(false)` is not enough (`RestoreAt = init`). The hart finishes doing so while still START_PENDING and misses every `pmp_set_global` other harts issue before it becomes STARTED. For example, right after an enclave is created, or right after a resume revokes a share, that hart alone becomes STARTED with a stale permission.

  Choosing the targets of the re-send by the state the loop sees is not enough either (`Resend = state`). The counterexample: hart A restarts and programs its entries (START_PENDING). Hart B starts the `pmp_set_global` of a create, and A is not in the mask. A becomes STARTED. The loop of B sees that A is not START_PENDING and skips it. A has no entry for the new enclave, and the host on A can read and write the ADM while the enclave runs.
+ *Race between a stop and a resume (\#13, new)*: between the moment a stopping hart drops the lock and the end of its `context_switch_to_host`, the host can, from another hart:
  - resume the enclave. The revocation of the resume (`pmp_set_global(NO_PERM)`) completes first, and the stopping hart then grants RW on every hart. The enclave runs again while host harts hold RW (E1, B2).
  - destroy it and create a new enclave with the same ID. The late `context_switch_to_host` reads the `share` of the new enclave (RW right after create) and grants RW on its ADM on every hart (A1). This also happens with an exit or an interrupt-driven stop, which request no sharing.

  The host only needs to issue a resume or a destroy right after the stop. Though not modeled, the swap of the register context in `threads[0]` also races with the swap on the resume side.
+ *Writable share with NULL metadata (\#12, new)*: `stop_enclave_with_share` rejects this call only after it has set the state to STOPPED (`n_thread = 0`). The enclave receives the error and keeps running. From another hart, the host can then:
  - resume it, so that the same enclave runs on two harts (D2d, A2);
  - destroy the running enclave. What the enclave writes after zeroing reaches the host when the region is released (A9, E1).

  The runtime's `dispatch_edgecall_ocall_with_share` passes the arguments of the eapp to the SM as they are, so a single call by the eapp is enough to take this path.
+ *Double destroy (\#11)*: DESTROYING also passes the guard of destroy, so two destroys for the same ID can proceed on two harts at once. The late one zeroes the ADM of a new enclave created with the same ID after the other destroy completed, and unsets its entry. The ADM of the new enclave is exposed to the host while it runs (A2, E1). The host alone suffices, but it needs a timing in which the late destroy interleaves with the creation and execution of the new enclave. Inherited from the original Keystone.
+ *Residual metadata (\#5)*: metadata passed with a stop sharing NONE or RONLY remains and is used by the validation of a later writable share. A region placed by the host with a UID the enclave did not pass this time still passes validation (B4, B5).
+ *Validation failure (\#1)*: the enclave is not destroyed and remains RUNNING with `n_thread = 1`. It never executes again either, so the weaker form of B3 holds. This is limited to a resource problem: the ADM is neither zeroed nor are its PMP entries released.
+ *`share_type` (\#6)*: on its own, it violates no property.

=== Caveats

- The counterexamples are counterexamples of the model. The checks on QEMU are in Section 10.5 (\#7 and \#13). The behavior of OpenSBI is based on reading the 1.1 source used by the build.
- The EPM and the UTM are not modeled. The part of create before the call of `pmp_set_global` is one transition.
- In `pmp_set_global`, writing the record and reading the mask are one transition. The memory ordering between the two (`pmp_lock` and `mb()`) is an assumption outside the model. The delivery to the harts of the mask is one transition per hart, and a hart that left the mask before the delivery does not apply the update.
- The re-send loop looks at the harts in numerical order; the model looks at them in any order.
- The race of the re-send has a narrow time window; reproducing it on QEMU was not attempted.
- The fix flags model one possible fix each; they are not fixes of the implementation. Every modeled difference (\#1, \#5, \#6, \#7, \#11, \#12 and \#13) is also fixed in the implementation.

== 10.4 Checking the implementation with CBMC

The harnesses are in `verif/cbmc/`. `make cbmc` checks `validate_adm_regions` of `sm/src/adm.c`, the enclave-side library of `sdk/src/edge/adm.c` and the region allocator of `sm/src/pmp.c` as they are with CBMC.

=== The validation function (B6, B7)

For `validate_adm_regions`, the ADM is memory of size S (below 2#super[40]) with arbitrary contents, and the metadata are 16 arbitrary entries. The harnesses compute in 128 bits so that wrap-around cannot hide a violation.

#table(
  columns: (2fr, 4fr, 8fr),
  table.header([*Property*], [*Harness and bounds*], [*Result*]),
  [B7], [`b7.c`; any S > 0, at most 16 regions, any metadata], [No read outside the ADM, no write to the ADM, no overflow, termination (the loops are unwound far enough)],
  [B6], [`b6.c`; S at least the ADM header, at most `N_MAX` regions, distinct metadata UIDs], [W1–W12 hold (checked with `N_MAX` = 2–6). A valid ADM with two regions is shown to exist, so the check is not vacuous. Metadata with a duplicated UID is checked separately never to be accepted (`-DDUP_META`, `N_MAX` = 2)],
)

Before the fix, the following counterexamples were found (`N_MAX` = 2).

- B7: overflow of `data->offset + data->size` (\#4) and of `meta.size` times the element width. With S smaller than the ADM header, the header is read out of bounds, but an ADM is page-granular, so this does not happen in practice.
- B6: W2 (duplicate UIDs, a missing expected region; \#2), W3 (a region header overlapping the ADM header), W4 (overflow; \#4), W6 (headers overlapping each other, headers and data, data and data; \#4), W7 (the type is not compared; \#3), and W9 (overflow of the multiplication). W1, W5 and W8 held.

W4 is the most dangerous. Writing a huge `size` into a region of a variable-length array type makes `offset + size` wrap around and pass validation. An enclave that trusts this `size` reads and writes outside the ADM.

The fix (`adm.c`):

- Compare the number of regions with the number of used metadata entries, and reject duplicate and unknown UIDs (W2).
- A region header lies after the ADM header, inside the ADM, and does not overlap the previous one (W3, W5, W6).
- Check the range of a data body as `size > S || offset > S - size` (W4), and that it overlaps neither the ADM header, nor any region header, nor any other data body (W6).
- Compare the type in the region header with the metadata (W7), and the size by division (W9).
- Check that a data body follows its region header (W10), that `free_list` lies after every region (W11), and that the region headers and `free_list` are at multiples of 8 (W12) (\#16).
- Read each field of the ADM once.
- New error codes `ADM_REGION_COUNT_MISMATCH`, `ADM_INVALID_UID`, `ADM_TYPE_MISMATCH`, `ADM_HEADER_NOT_ADJACENT`, `ADM_MISALIGNED` and `ADM_INVALID_FREE_LIST`.

The validation function before W10–W12 were added has counterexamples for these three conditions (`N_MAX` = 2). The first ADM, built by the host-side SDK, and the ADMs built by the corrected library pass the checks including W10–W12 (next subsection and Section 10.5). The runtime passes every region as metadata on each stop.

Bounds: B6 holds for at most 6 regions (about 68 minutes with 6). Each condition relates at most two regions, so a larger bound is not expected to reveal a new kind of violation, but this is not proved. With 16 regions, the many reads at symbolic positions kept the check from finishing. B7 covers up to 16 regions (about 28 minutes).

=== The enclave-side library (\#16)

`admlib.c` starts from any ADM that the validation function of the SM accepts (with any metadata), calls one of `adm_new_region`, `adm_set_bytes`, `adm_remove_region` and `adm_reset` of `sdk/src/edge/adm.c` once with any arguments, and checks the following. The validation function is that of `sm/src/adm.c`, as it is.

- Nothing is written outside the ADM. The ADM is the first S bytes (any S) of a buffer of `S_FIX` bytes; accesses outside the buffer are caught by the pointer checks of CBMC, and a byte after the ADM is the same before and after the operation.
- The other regions remain, at the same place and with the same contents.
- After the operation, the ADM passes validation again with the metadata of `adm_gen_type_info`: the library preserves WF.
- An added region is inside the ADM and is found by its UID. With `adm_set_bytes`, the region holds the given bytes.
- After `adm_reset`, there is no region, everything after the header is 0, and the ADM passes validation (the other regions are not required to remain).

The region to add has a non-zero UID and a size that fits its type. `memset` and `memcpy` are replaced by byte loops.

#table(
  columns: (3fr, 2fr, 8fr),
  table.header([*Regions before the operation*], [*`S_FIX`*], [*Result*]),
  [at most 1], [304], [Every property holds (about 4 minutes). An addition, a setting of bytes, a removal, and a reset of an ADM with a region are reached],
  [at most 2], [336], [Every property holds (about 9 minutes). In addition to the above, the removal of a region other than the last is reached],
  [at most 4], [512], [As above (about 90 minutes)],
  [at most 6], [640], [As above (about 7 hours 46 minutes)],
)

The times include the reachability runs. `make cbmc` uses the setting of the second row (`LIB_N` and `LIB_S` change it). With the library and the validation function before the fix, the setting of the first row gives counterexamples, including writes outside the ADM (34 of 644 properties, with the harness for addition and removal only).

This check found two more errors, both fixed.

- An ADM with a region header at an offset that is not a multiple of 8 passed validation, and removing its last region left `free_list` misaligned. The position of the region headers was added to W12. The SM reads the region headers in M-mode too, so aligned headers are also what the SM wants.
- `adm_gen_type_info` and the runtime put the size in bytes into the metadata. The SM compares the size of a static-length array (`STATIC_ARR_16/32/64`) as a number of elements (`SizeOK`, Section 6.2), so that an ADM with a region of these types always failed validation after a writable share. The library now gives the number of elements, and the runtime calls `adm_gen_type_info`.

Bounds: the number of regions and the size of the ADM are small (table above). The allocator inside the ADM (`adm_alloc.c`) is out of scope (Section 1.2).

A removal does not move the other regions. Before the fix it moved the later regions down, which invalidated, on every removal, the pointers into them, the offsets inside the ADM passed to the host, and the pointers kept by the allocator. In exchange, the space of a removed region that is not the last one is reused only once all the later regions are removed. The region the runtime uses for syscalls is normally the last one, so such holes do not accumulate.

The host-side SDK (`sdk/src/host/AdditionalDataMemory.cpp`) was also wrong. When it builds the first ADM, its estimate of the required size is smaller than the layout it writes, and it writes past the end of an ADM that is almost full (regions of 64 and 16 bytes in an ADM of 400 bytes go 8 bytes past the end; confirmed with a unit test). The memory written is the host's own and no property of Section 7 is concerned; the check now uses the same computation as the layout.

=== PMP region allocation (C1, C2, C3, C5)

`pmp_regions.c` includes `sm/src/pmp.c` as it is and replaces the PMP CSRs with arrays (with the patched OpenSBI headers the SM is built with). After the SM and OS regions are created as in `sm_init`, K arbitrary operations follow (allocation with `PMP_PRI_ANY` or `PMP_PRI_BOTTOM`, or a free of any region but the SM and OS regions). Then, for any region created with `PMP_PRI_ANY` / `PMP_PRI_TOP`, the harness checks the following, with 56-bit physical addresses and 54-bit pmpaddr.

- C1: its address range overlaps no other region but the OS region.
- C2: its entries come after the SM entry and before the OS entry.
- C3: it shares no entry with another region. Programmed alone, its entry matches exactly `[addr, addr + size)` (by the RISC-V TOR / NAPOT matching rules).
- C5: when an allocation fails, `reg_bitmap` and `region_def_bitmap` are unchanged.

Before the fix, C2 and C3 (shared entries) had counterexamples (Section 8 \#14): allocate and free a UTM, allocate three TOR regions (registers 1–6), and the next NAPOT region receives register 7, shared with the OS. After the fix, which leaves the lowest-priority register out of the search for `PMP_PRI_ANY`, every property holds with K = 6 (about 27 minutes) and K = 8 (about 88 minutes). Both TOR and NAPOT regions, and frees in an order other than that of the allocations, are reached. The encoding of C3 (the TOR lower-bound register, the NAPOT mask) and C5 held before the fix.

The cleanup of `create_enclave` on failure (C5 on the enclave side) was checked by code reading: it frees the regions it allocated in reverse order and unsets the entries it set on every hart before releasing the eid.

=== Addresses in SBI arguments (H8, \#15)

`h8.c` calls one of the SBI functions of `sm-sbi.c` (create, destroy, run, resume, attest, sealing key, stop, exit, stop with sharing, plugin, random) once, with arbitrary arguments and an enclave table with arbitrary contents. `enclave.c`, `sm-sbi.c` and `plugins/` are the real code; the PMP, the context switch, key derivation and signing, the validation function and the hash are stubs. The only object that stands for physical memory is the EPM of the calling enclave, so the pointer check fails if the SM reads or writes an argument value as an address. The harness checks the following.

- The copy through MPRV (`copy_to_sm`, `copy_from_sm`) reads and writes, on the side of the SM, only inside the buffer (the address on the side of the caller is checked by the PMP with the privilege of the caller).
- Key derivation and signing read and write only inside the EPM of the caller or inside the SM. The identifier is at most 1024 bytes long.
- `sbi_memset` is applied only inside a range registered as a PMP region (the UTM at creation, the EPM and the ADM at destruction).

#table(
  columns: (4fr, 8fr),
  table.header([*Check*], [*Result*]),
  [`h8.c`. Each entry of the enclave table (`EID` = 0 to 15) and ids outside the table (16, 2#super[32] − 1)], [Every property holds (529 properties each, 1 to 3 minutes per run). The derivation of a sealing key, the output of a report, creation, destruction, a stop with metadata and the output of a plugin are reached],
  [`mprv.c`. The copy loops of `mprv.h`. At most 80 bytes, every combination of the alignments of the two addresses], [The copy proceeds in order without a gap, stays inside the length, stops at a failure, and covers the whole length on success. An aligned and an unaligned copy are reached],
  [Control: the SM without the check of `get_sealing_key` (`verif/qemu/unfix-15.patch`)], [`h8.c` fails (7 properties, among them the destination of the key, the source of the identifier and the length of the identifier)],
)

The index into the enclave table is a constant in each run (the check does not finish with a symbolic one). `make cbmc` uses `EID` = 0, 15, 16 (`H8_EIDS` changes it). In the check of the copy loops, the addresses are represented by values below 8 (the loops use the addresses only to decide the alignment).

== 10.5 Checks on QEMU

`verif/qemu/run-tests.sh [BIOS]` boots the image of `run-virt` on a fresh overlay (with the SM replaced by `BIOS` if given), copies the test apps `admfail.ke` and `admpeek.ke` into it, and runs them.

- `admpeek` (\#7): after creating an enclave, stop and restart a CPU with CPU hotplug, and read the host's mapping of the ADM from that CPU.
- `admfail` (\#1): while the enclave shares the ADM read-write, the host breaks the ADM header. Validation fails on resume and the SM destroys the enclave. The check is whether the destroy by the SDK then succeeds.

#table(
  columns: (4fr, 6fr, 6fr),
  table.header([*Check*], [*Fixed SM*], [*SM before the fixes (`ee980f5`)*]),
  [`admpeek` (CPU 1, CPU 3)], [Access to the ADM faults before and after the restart], [After the restart, the host reads the ADM (\#7 reproduced)],
  [`admfail` (not broken)], [Completes normally], [Completes normally],
  [`admfail` (broken)], [The SM destroys the enclave (`exceeded maximum number of slot`), and the SDK's destroy succeeds. Eight repetitions do not exhaust any resource], [The SM keeps the enclave; later creations fail],
)

With the fixed SM, enclaves pinned to a restarted CPU (`hello-adm.ke`, `admfail.ke`) also run. The results of the table are the same with the SM after the correction of the re-send. \#13 is covered in the next subsection. For \#15, the eapp of `admfail` asks for a sealing key through the unmodified runtime. The fixed SM returns a key for an 8-byte identifier and refuses a 2000-byte one; the SM before the fix of \#15 (`fb37c88`) accepts 2000 bytes as well. The unmodified runtime cannot pass an address outside the EPM (it only passes its own buffers). So `verif/qemu/run-seal.sh [BIOS]` rebuilds `admfail` with a runtime that passes the start of the UTM as the destination of the key (`verif/qemu/seal-outside.patch`) and runs it. The fixed SM refuses the request. The SM without the check of `get_sealing_key` only (`verif/qemu/unfix-15.patch`) accepts it, and the key is written to the UTM. For \#16, the eapp of `admfail` makes a region after the region the runtime uses for syscalls, and checks its position and contents after the next syscall (for which the runtime removes its region and makes it again). After the fix, both are kept and the following resume from a writable share succeeds. With the library and the SM before the fix (`fb37c88`), validation fails (`region are overlapped`) although the host does not break the ADM, and the enclave is destroyed. `iotest-adm.ke` and `iotest-adm-nocpy.ke`, which delegate file I/O through the ADM (the latter with the allocator inside the ADM), also run with the fixed SM, library and runtime. The new driver assumes the fixed SM (with an SM before the fixes, the driver frees memory the SM still protects after a validation failure).

=== The race between stop and resume (\#13)

`verif/qemu/run-race.sh [BIOS]` runs `admrace.ke`. The eapp stops with the ADM shared for writing, then stops without a share, in a loop. The host issues resume from two threads (CPU 1 and CPU 2). While one of them runs the enclave up to the stop with the share, the other keeps issuing resume, so that a resume from another hart arrives right after the stop. The thread whose resume was accepted runs the enclave alone up to the stop without a share, and reads the ADM from every CPU there. While the enclave is stopped without a share, no CPU may be able to read it. At the start, without a race, the program checks that every CPU can read the ADM during a stop with the share and that no CPU can during a stop without a share.

#table(
  columns: (5fr, 11fr),
  table.header([*SM*], [*Result*]),
  [Fixed], [20,000 checks complete, and the ADM is never readable (in both of 2 runs)],
  [Only the fix of \#13 removed (`verif/qemu/unfix-13.patch`)], [In all of 5 runs, the CPU running the enclave stops answering within seconds of the start (Linux reports a hard LOCKUP). No run reaches 1,000 checks],
)

With the SM without the fix, the stopped hart was inspected through the QEMU monitor (once): it is halted in M mode, `mcause` is an instruction access fault, and `mepc` and `mtval` are an address of the runtime (`0xffffffffc0000040`). The register context of the enclave and the PMP setting of that hart disagree. The likely cause is that a resume was accepted before `context_switch_to_host` of the stopping side finished, and the two switched `threads[0]` and the PMP concurrently (the race on the register context that Section 10.3 mentions as not modeled).

So the cause of \#13 (a resume accepted in the middle of a stop), and that the fix removes it, are confirmed on QEMU. The PMP exposure of the model's counterexample (the host holds RW after the enclave is resumed) was not observed, because the machine stops before it.

== 10.6 Remaining work

+ The hash of the EPM (`validate_and_hash_enclave`, which walks the page table) and the assembly of the copy (`mprv.S`), which are stubs in the check of H8 (Section 10.4), are not checked for the range they read and write.

#rocq-appendix("en")
