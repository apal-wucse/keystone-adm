(** * Region placement and PMP entry priority (group C)

    Rocq counterpart of [verif/tla/ADMRegions.tla] (MODEL.typ Section
    7.3).  The design model of ADM.v is extended with the physical placement
    of each enclave's private memory (EPM) and ADM, and the permission a
    hart actually gets at an address is derived from the PMP entries:

<<
    priority   entry                 permission on hart h
    highest    SM region             none
    middle     EPM of enclave e      all if h runs e, otherwise none
    middle     ADM of enclave e      pmp e h
    lowest     host OS / UTM         (not modeled)
>>

    The relative priority of the middle entries is NOT fixed: the
    properties quantify over every entry that matches an address, so they
    hold for any order in which the SM allocates entries.

    Compared with the TLA+ module, addresses are an arbitrary type and a
    region is an arbitrary non-empty set of addresses (TLA+: a non-empty
    interval of a few pages).  Contiguity and alignment play no role in the
    properties below; C3 (TOR / NAPOT encoding) is represented only as "an
    entry matches exactly its region". *)

From ADMVerif Require Import ADM.

Section Regions.

Variables Hart EID Data Meta Addr : Type.
Hypothesis hart_eq_dec : forall x y : Hart, {x = y} + {x <> y}.
Hypothesis eid_eq_dec : forall x y : EID, {x = y} + {x <> y}.
Variable Zero : Data.
Variable WF : Data -> Meta -> Prop.
(** Addresses occupied by the SM. *)
Variable SMRegion : Addr -> Prop.

Local Notation St := (ADM.St Hart EID Data Meta).
Local Notation state := (ADM.state Hart EID Data Meta).
Local Notation phase := (ADM.phase Hart EID Data Meta).
Local Notation ctx := (ADM.ctx Hart EID Data Meta).
Local Notation pmp := (ADM.pmp Hart EID Data Meta).
Local Notation Host := (ADM.Host EID).
Local Notation init := (ADM.init Hart EID Data Meta Zero).
Local Notation step :=
  (ADM.step Hart EID Data Meta hart_eq_dec eid_eq_dec Zero WF).
Local Notation reachable :=
  (ADM.reachable Hart EID Data Meta hart_eq_dec eid_eq_dec Zero WF).
Local Notation do_create := (ADM.do_create Hart EID Data Meta eid_eq_dec).
Local Notation do_destroy_free :=
  (ADM.do_destroy_free Hart EID Data Meta eid_eq_dec).

(** ** Regions *)

Definition Region := Addr -> Prop.
Definition Empty (R : Region) : Prop := forall a, ~ R a.
Definition Disjoint (R1 R2 : Region) : Prop := forall a, R1 a -> R2 a -> False.
Definition Union (R1 R2 : Region) : Region := fun a => R1 a \/ R2 a.

Record Rg := mkRg {
  epm : EID -> Region;   (* pages of e's private memory (empty if e does not exist) *)
  adm : EID -> Region    (* pages of e's ADM            (empty if e does not exist) *)
}.

Definition Prot (r : Rg) (e : EID) : Region := Union (epm r e) (adm r e).

Definition setR (r : Rg) (e : EID) (Re Ra : Region) : Rg :=
  mkRg (fun x => if eid_eq_dec x e then Re else epm r x)
       (fun x => if eid_eq_dec x e then Ra else adm r x).

Definition emptyR : Region := fun _ => False.

(** C1: the new regions overlap neither each other, the SM, nor any region
    of another enclave. *)
Definition NoOverlap (r : Rg) (e : EID) (Re Ra : Region) : Prop :=
  Disjoint Re Ra /\ Disjoint (Union Re Ra) SMRegion /\
  forall e2, e2 <> e -> Disjoint (Union Re Ra) (Prot r e2).

(** ** Transitions ([NextR] of ADMRegions.tla)

    Create picks the regions; releasing the PMP entries returns both
    regions; every other step of the design model leaves them unchanged.
    [step_cases] below shows that the third constructor covers exactly the
    remaining steps of ADM.v. *)

Inductive stepR : St * Rg -> St * Rg -> Prop :=
| SR_Create s r h e d m0 Re Ra :
    ctx s h = Host -> state s e = INVALID ->
    NoOverlap r e Re Ra -> ~ Empty Re -> ~ Empty Ra ->
    stepR (s, r) (do_create s e d m0, setR r e Re Ra)
| SR_DestroyFree s r e :
    state s e = DESTROYING -> phase s e = ZEROED ->
    stepR (s, r) (do_destroy_free s e, setR r e emptyR emptyR)
| SR_Other s s' r :
    step s s' ->
    (forall e, state s e = INVALID <-> state s' e = INVALID) ->
    stepR (s, r) (s', r).

Inductive reachableR : St * Rg -> Prop :=
| RR_init : reachableR (init, mkRg (fun _ => emptyR) (fun _ => emptyR))
| RR_step p p' : reachableR p -> stepR p p' -> reachableR p'.

Lemma stepR_step : forall p p', stepR p p' -> step (fst p) (fst p').
Proof.
  intros p p' H. destruct H; cbn.
  - eapply S_Create; eassumption.
  - eapply S_DestroyFree; eassumption.
  - assumption.
Qed.

Lemma reachableR_reachable : forall p, reachableR p -> reachable (fst p).
Proof.
  induction 1.
  - apply R_init.
  - eapply R_step; [eassumption | apply stepR_step; assumption].
Qed.

(** Every step of the design model is a Create, a DestroyFree, or a step
    that keeps the set of existing enclaves; so [stepR] has a counterpart
    for every step of ADM.v. *)
Lemma step_cases : forall s s', reachable s -> step s s' ->
  (exists h e d m0, ctx s h = Host /\ state s e = INVALID /\
                    s' = do_create s e d m0) \/
  (exists e, state s e = DESTROYING /\ phase s e = ZEROED /\
             s' = do_destroy_free s e) \/
  (forall e, state s e = INVALID <-> state s' e = INVALID).
Proof.
  intros s s' R H.
  destruct H as [h e d m0 | | | | | | | | | | | | e | |];
    [left; exists h, e, d, m0; auto | ..
    | right; left; exists e; auto | | ];
    right; right; intro x;
    unfold do_begin, do_revoke_step, do_revoke_done, do_validate_pass,
      do_validate_fail, do_stop, do_destroy_begin, do_destroy_zero,
      do_enc_write, do_adv_write, updE; cbn;
    try destruct (eid_eq_dec x e); subst; try tauto;
    try (split; intro; intuition congruence).
  (* ValidateFail: an enclave under validation is RUNNING *)
  pose proof (running_of_phase Hart EID Data Meta hart_eq_dec eid_eq_dec
                Zero WF s R e (or_intror H)).
  split; intro; congruence.
Qed.

(** ** Properties *)

(** C1: protected regions are pairwise disjoint. *)
Definition C1_RegionsDisjoint (r : Rg) : Prop :=
  (forall e, Disjoint (epm r e) (adm r e) /\ Disjoint (Prot r e) SMRegion) /\
  (forall e1 e2, e1 <> e2 -> Disjoint (Prot r e1) (Prot r e2)).

(** Regions exist exactly while the enclave exists. *)
Definition C_RegionsFollowLifecycle (s : St) (r : Rg) : Prop :=
  forall e, state s e = INVALID <-> (Empty (epm r e) /\ Empty (adm r e)).

Lemma C1_reachable : forall p, reachableR p -> C1_RegionsDisjoint (snd p).
Proof.
  unfold C1_RegionsDisjoint, Disjoint, Prot, Union.
  induction 1 as [|p p' _ [IH1 IH2] H].
  - cbn. unfold emptyR. firstorder.
  - destruct H as [s r h e d m0 Re Ra _ _ [N1 [N2 N3]] _ _ | s r e _ _ | ];
      cbn in *; unfold Disjoint, Union, emptyR in *; [split | split | auto].
    + intro x. destruct (eid_eq_dec x e); [subst; split; auto | apply IH1].
    + intros e1 e2 Hne a.
      destruct (eid_eq_dec e1 e), (eid_eq_dec e2 e); subst;
        [congruence | | | apply (IH2 e1 e2 Hne a)].
      * exact (N3 e2 (fun E => Hne (eq_sym E)) a).
      * intros H1 H2. exact (N3 e1 Hne a H2 H1).
    + intro x. destruct (eid_eq_dec x e); [tauto | apply IH1].
    + intros e1 e2 Hne a.
      destruct (eid_eq_dec e1 e), (eid_eq_dec e2 e); try tauto.
      apply (IH2 e1 e2 Hne a).
Qed.

Lemma lifecycle_reachable : forall p, reachableR p ->
  C_RegionsFollowLifecycle (fst p) (snd p).
Proof.
  unfold C_RegionsFollowLifecycle, Empty.
  induction 1 as [|p p' _ IH H].
  - cbn. unfold emptyR. tauto.
  - destruct H as [s r h e d m0 Re Ra _ _ _ NRe NRa | s r e _ _ | s s' r _ Hi];
      cbn in *; intro x.
    + unfold updE; cbn. destruct (eid_eq_dec x e); [subst | apply IH].
      split; [discriminate | intros [E _]; contradiction].
    + unfold updE; cbn. destruct (eid_eq_dec x e); [subst | apply IH].
      unfold emptyR. tauto.
    + destruct (Hi x) as [Hi1 Hi2]. destruct (IH x) as [IH1 IH2].
      split; intro Hx; [apply IH1, Hi2, Hx | apply Hi1, IH2, Hx].
Qed.

(** ** Permissions derived from the PMP entries *)

Inductive MPerm := MP_ALL | MP (p : Perm).

Definition EpmPerm (s : St) (h : Hart) (e : EID) : MPerm :=
  match ctx s h with
  | ADM.Enc _ e' => if eid_eq_dec e' e then MP_ALL else MP P_NONE
  | _ => MP P_NONE
  end.

(** [MatchPerms s r h a p]: [p] is the permission of some middle-priority
    entry of hart [h] that matches address [a]. *)
Definition MatchPerms (s : St) (r : Rg) (h : Hart) (a : Addr) (p : MPerm)
  : Prop :=
  (exists e, epm r e a /\ p = EpmPerm s h e) \/
  (exists e, adm r e a /\ p = MP (pmp s e h)).

Section Derived.

Variables (s : St) (r : Rg).
Hypothesis C1 : C1_RegionsDisjoint r.

(** C2 (consequence): the SM region is matched by no enclave entry, so the
    highest-priority entry alone decides, and it grants nothing. *)
Lemma SMNotShadowed : forall a h p, SMRegion a -> ~ MatchPerms s r h a p.
Proof.
  intros a h p Ha [[e [He _]] | [e [He _]]];
    apply (proj2 (proj1 C1 e) a); unfold Prot, Union; auto.
Qed.

(** Whatever the priority order of the middle entries, a page of e's EPM
    is matched only by e's EPM entry. *)
Lemma EPMIsolated : forall e h a p,
  epm r e a -> (MatchPerms s r h a p <-> p = EpmPerm s h e).
Proof.
  intros e h a p Ha. split.
  - intros [[e' [He' P]] | [e' [He' _]]];
      destruct (eid_eq_dec e' e); subst; auto; exfalso.
    + apply (proj2 C1 e' e n a); unfold Prot, Union; auto.
    + apply (proj1 (proj1 C1 e) a); auto.
    + apply (proj2 C1 e' e n a); unfold Prot, Union; auto.
  - intro P. left. eauto.
Qed.

(** Whatever the priority order, the permission that applies to every page
    of e's ADM is exactly [pmp s e h].  This lifts groups A and B from the
    abstract [pmp] to physical addresses. *)
Lemma ADMPermFaithful : forall e h a p,
  adm r e a -> (MatchPerms s r h a p <-> p = MP (pmp s e h)).
Proof.
  intros e h a p Ha. split.
  - intros [[e' [He' _]] | [e' [He' P]]];
      destruct (eid_eq_dec e' e); subst; auto; exfalso.
    + apply (proj1 (proj1 C1 e) a); auto.
    + apply (proj2 C1 e' e n a); unfold Prot, Union; auto.
    + apply (proj2 C1 e' e n a); unfold Prot, Union; auto.
  - intro P. right. eauto.
Qed.

End Derived.

(** ** Theorems for every reachable state *)

Theorem C1_RegionsDisjoint_holds : forall s r,
  reachableR (s, r) -> C1_RegionsDisjoint r.
Proof. intros s r H. exact (C1_reachable _ H). Qed.

Theorem C_RegionsFollowLifecycle_holds : forall s r,
  reachableR (s, r) -> C_RegionsFollowLifecycle s r.
Proof. intros s r H. exact (lifecycle_reachable _ H). Qed.

Theorem C2_SMNotShadowed : forall s r a h p,
  reachableR (s, r) -> SMRegion a -> ~ MatchPerms s r h a p.
Proof.
  intros s r a h p H. apply SMNotShadowed. exact (C1_reachable _ H).
Qed.

Theorem C_EPMIsolated : forall s r e h a p,
  reachableR (s, r) -> epm r e a -> (MatchPerms s r h a p <-> p = EpmPerm s h e).
Proof.
  intros s r e h a p H. apply EPMIsolated. exact (C1_reachable _ H).
Qed.

Theorem C_ADMPermFaithful : forall s r e h a p,
  reachableR (s, r) -> adm r e a ->
  (MatchPerms s r h a p <-> p = MP (pmp s e h)).
Proof.
  intros s r e h a p H. apply ADMPermFaithful. exact (C1_reachable _ H).
Qed.

(** The properties of ADM.v carry over to the extended model. *)
Theorem reachableR_inv : forall s r,
  reachableR (s, r) -> Inv Hart EID Data Meta s.
Proof.
  intros s r H. apply (inv_reachable Hart EID Data Meta hart_eq_dec eid_eq_dec
                         Zero WF s).
  exact (reachableR_reachable _ H).
Qed.

End Regions.
