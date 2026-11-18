(** * Design model of Additional Data Memory (ADM) / Protected Sharing

    Rocq counterpart of [verif/tla/ADM.tla] (MODEL.typ Sections 3-5 and 7).
    The TLA+ model is checked by TLC for small fixed configurations; here
    the same properties are proved for

    - any number of harts and enclaves (arbitrary types with decidable
      equality),
    - any set of ADM contents and metadata,
    - any validation predicate [WF] (the TLA+ model fixes [WF(d, m)] to
      [d = m]).

    Differences from the TLA+ text:

    - [ValidatePass] / [ValidateFail] name the runner hart in their guard
      ([runner s e = Some h]).  TLA+ writes [runner[e]] directly.  The two
      agree on reachable states because of [validating_has_runner] below.
    - "No metadata" / "no hart" are [None].

    Only the Rocq core library is used; there are no axioms. *)

Section ADM.

Variables Hart EID Data Meta : Type.
Hypothesis hart_eq_dec : forall x y : Hart, {x = y} + {x <> y}.
Hypothesis eid_eq_dec : forall x y : EID, {x = y} + {x <> y}.

Variable Zero : Data.
(** Structural predicate of MODEL.typ Section 6.2, kept abstract. *)
Variable WF : Data -> Meta -> Prop.

Inductive EState := INVALID | FRESH | RUNNING | STOPPED | DESTROYING.
Inductive Share := SH_NONE | SH_RONLY | SH_RW.
Inductive Perm := P_NONE | P_R | P_RW.
Inductive Phase := IDLE | REVOKING | VALIDATING | ZEROED.
Inductive StopKind :=
  K_none | K_share_none | K_share_ro | K_share_rw | K_invol | K_exit.
Inductive Ctx := Host | Enc (e : EID) | Sm (e : EID).

Record St := mkSt {
  state : EID -> EState;         (* lifecycle state *)
  share : EID -> Share;          (* sharing state *)
  meta : EID -> option Meta;     (* expected metadata *)
  phase : EID -> Phase;          (* SM-internal progress *)
  runner : EID -> option Hart;   (* hart executing / entering the enclave *)
  ctx : Hart -> Ctx;             (* context of each hart *)
  pmp : EID -> Hart -> Perm;     (* S/U-mode permission on the ADM *)
  mem : EID -> Data;             (* ADM contents *)
  (* ghost state *)
  secret : EID -> bool;          (* contents not disclosed by the enclave *)
  lastStop : EID -> StopKind;    (* how the enclave stopped most recently *)
  dirty : EID -> bool;           (* a non-owner could have written *)
  sentMeta : EID -> option Meta; (* metadata from a legitimate source *)
  dead : EID -> bool;            (* validation failed *)
  snap : EID -> Data             (* contents at the most recent stop *)
}.

Definition updE {A} (f : EID -> A) (e : EID) (v : A) : EID -> A :=
  fun x => if eid_eq_dec x e then v else f x.
Definition updH {A} (f : Hart -> A) (h : Hart) (v : A) : Hart -> A :=
  fun x => if hart_eq_dec x h then v else f x.

Definition HostPerm (sh : Share) : Perm :=
  match sh with SH_NONE => P_NONE | SH_RONLY => P_R | SH_RW => P_RW end.

Definition PermLeq (p q : Perm) : Prop :=
  match p, q with
  | P_NONE, _ => True
  | P_R, P_NONE => False
  | P_R, _ => True
  | P_RW, P_RW => True
  | P_RW, _ => False
  end.

Definition WFo (d : Data) (m : option Meta) : Prop :=
  match m with Some m => WF d m | None => False end.

Definition kind_of (sh : Share) : StopKind :=
  match sh with
  | SH_NONE => K_share_none
  | SH_RONLY => K_share_ro
  | SH_RW => K_share_rw
  end.

Definition init : St :=
  mkSt (fun _ => INVALID) (fun _ => SH_NONE) (fun _ => None) (fun _ => IDLE)
       (fun _ => None) (fun _ => Host) (fun _ _ => P_NONE) (fun _ => Zero)
       (fun _ => false) (fun _ => K_none) (fun _ => false) (fun _ => None)
       (fun _ => false) (fun _ => Zero).

(** ** Post-states of the transitions (MODEL.typ Section 5) *)

Definition do_create (s : St) (e : EID) (d : Data) (m0 : Meta) : St :=
  mkSt (updE (state s) e FRESH) (updE (share s) e SH_RW)
       (updE (meta s) e (Some m0)) (phase s) (runner s) (ctx s)
       (updE (pmp s) e (fun _ => P_NONE)) (updE (mem s) e d)
       (updE (secret s) e false) (updE (lastStop s) e K_none)
       (updE (dirty s) e true) (updE (sentMeta s) e (Some m0))
       (updE (dead s) e false) (snap s).

(** Common part of RunBegin ([p = VALIDATING]) and ResumeBegin
    ([p = REVOKING]). *)
Definition do_begin (s : St) (h : Hart) (e : EID) (p : Phase) : St :=
  mkSt (updE (state s) e RUNNING) (share s) (meta s) (updE (phase s) e p)
       (updE (runner s) e (Some h)) (updH (ctx s) h (Sm e)) (pmp s) (mem s)
       (secret s) (lastStop s) (dirty s) (sentMeta s) (dead s) (snap s).

Definition do_revoke_step (s : St) (h : Hart) (e : EID) : St :=
  mkSt (state s) (share s) (meta s) (phase s) (runner s) (ctx s)
       (updE (pmp s) e (updH (pmp s e) h P_NONE)) (mem s)
       (secret s) (lastStop s) (dirty s) (sentMeta s) (dead s) (snap s).

Definition do_revoke_done (s : St) (e : EID) : St :=
  mkSt (state s) (share s) (meta s) (updE (phase s) e VALIDATING) (runner s)
       (ctx s) (pmp s) (mem s)
       (secret s) (lastStop s) (dirty s) (sentMeta s) (dead s) (snap s).

Definition do_validate_pass (s : St) (e : EID) (h : Hart) : St :=
  mkSt (state s) (updE (share s) e SH_NONE) (updE (meta s) e None)
       (updE (phase s) e IDLE) (runner s) (updH (ctx s) h (Enc e))
       (updE (pmp s) e (updH (pmp s e) h P_RW)) (mem s)
       (secret s) (lastStop s)
       (* dirty is cleared only when the contents were actually checked *)
       (match share s e with
        | SH_RW => updE (dirty s) e false
        | _ => dirty s
        end)
       (sentMeta s) (dead s) (snap s).

Definition do_validate_fail (s : St) (e : EID) (h : Hart) : St :=
  mkSt (updE (state s) e DESTROYING) (share s) (meta s)
       (updE (phase s) e IDLE) (updE (runner s) e None)
       (updH (ctx s) h Host) (pmp s) (mem s)
       (secret s) (lastStop s) (dirty s) (sentMeta s)
       (updE (dead s) e true) (snap s).

Definition do_stop (s : St) (h : Hart) (e : EID) (sh : Share)
           (m : option Meta) (k : StopKind) : St :=
  mkSt (updE (state s) e STOPPED) (updE (share s) e sh) (updE (meta s) e m)
       (phase s) (updE (runner s) e None) (updH (ctx s) h Host)
       (match sh with
        | SH_NONE => updE (pmp s) e (updH (pmp s e) h P_NONE)
        | _ => updE (pmp s) e (fun _ => HostPerm sh)
        end)
       (mem s)
       (* sharing discloses the contents by the enclave's own intent *)
       (match sh with
        | SH_NONE => secret s
        | _ => updE (secret s) e false
        end)
       (updE (lastStop s) e k)
       (match sh with SH_RW => updE (dirty s) e true | _ => dirty s end)
       (match sh with SH_RW => updE (sentMeta s) e m | _ => sentMeta s end)
       (dead s) (updE (snap s) e (mem s e)).

Definition do_destroy_begin (s : St) (e : EID) : St :=
  mkSt (updE (state s) e DESTROYING) (share s) (meta s) (phase s) (runner s)
       (ctx s) (pmp s) (mem s)
       (secret s) (lastStop s) (dirty s) (sentMeta s) (dead s) (snap s).

Definition do_destroy_zero (s : St) (e : EID) : St :=
  mkSt (state s) (share s) (meta s) (updE (phase s) e ZEROED) (runner s)
       (ctx s) (pmp s) (updE (mem s) e Zero)
       (updE (secret s) e false) (lastStop s) (dirty s) (sentMeta s)
       (dead s) (snap s).

Definition do_destroy_free (s : St) (e : EID) : St :=
  mkSt (updE (state s) e INVALID) (updE (share s) e SH_NONE)
       (updE (meta s) e None) (updE (phase s) e IDLE) (runner s) (ctx s)
       (updE (pmp s) e (fun _ => P_NONE)) (mem s)
       (secret s) (updE (lastStop s) e K_none) (updE (dirty s) e false)
       (updE (sentMeta s) e None) (dead s) (snap s).

Definition do_enc_write (s : St) (e : EID) (d : Data) : St :=
  mkSt (state s) (share s) (meta s) (phase s) (runner s) (ctx s) (pmp s)
       (updE (mem s) e d)
       (updE (secret s) e true) (lastStop s) (dirty s) (sentMeta s)
       (dead s) (snap s).

Definition do_adv_write (s : St) (e : EID) (d : Data) : St :=
  mkSt (state s) (share s) (meta s) (phase s) (runner s) (ctx s) (pmp s)
       (updE (mem s) e d)
       (updE (secret s) e false) (lastStop s) (updE (dirty s) e true)
       (sentMeta s) (dead s) (snap s).

(** Hart [h] is adversarial with respect to [e]: host, or another enclave. *)
Definition Adversarial (s : St) (h : Hart) (e : EID) : Prop :=
  ctx s h = Host \/ exists e2, e2 <> e /\ ctx s h = Enc e2.

(** ** Transition relation ([Next] of ADM.tla) *)

Inductive step (s : St) : St -> Prop :=
| S_Create h e d m0 :
    ctx s h = Host -> state s e = INVALID ->
    step s (do_create s e d m0)
| S_RunBegin h e :
    ctx s h = Host -> state s e = FRESH ->
    step s (do_begin s h e VALIDATING)
| S_ResumeBegin h e :
    ctx s h = Host -> state s e = STOPPED -> phase s e = IDLE ->
    step s (do_begin s h e REVOKING)
| S_RevokeStep h e :
    phase s e = REVOKING -> pmp s e h <> P_NONE ->
    step s (do_revoke_step s h e)
| S_RevokeDone e :
    phase s e = REVOKING -> (forall h, pmp s e h = P_NONE) ->
    step s (do_revoke_done s e)
| S_ValidatePass e h :
    phase s e = VALIDATING -> runner s e = Some h ->
    (share s e <> SH_RW \/ WFo (mem s e) (meta s e)) ->
    step s (do_validate_pass s e h)
| S_ValidateFail e h :
    phase s e = VALIDATING -> runner s e = Some h ->
    share s e = SH_RW -> ~ WFo (mem s e) (meta s e) ->
    step s (do_validate_fail s e h)
| S_StopShare h e sh m :
    ctx s h = Enc e -> state s e = RUNNING ->
    (sh = SH_RW <-> m <> None) ->
    step s (do_stop s h e sh m (kind_of sh))
| S_StopInvoluntary h e :
    ctx s h = Enc e -> state s e = RUNNING ->
    step s (do_stop s h e SH_NONE None K_invol)
| S_Exit h e :
    ctx s h = Enc e -> state s e = RUNNING ->
    step s (do_stop s h e SH_NONE None K_exit)
| S_DestroyBegin h e :
    ctx s h = Host -> (state s e = FRESH \/ state s e = STOPPED) ->
    phase s e = IDLE ->
    step s (do_destroy_begin s e)
| S_DestroyZero e :
    state s e = DESTROYING -> phase s e = IDLE ->
    step s (do_destroy_zero s e)
| S_DestroyFree e :
    state s e = DESTROYING -> phase s e = ZEROED ->
    step s (do_destroy_free s e)
| S_EncWrite h e d :
    ctx s h = Enc e -> pmp s e h = P_RW ->
    step s (do_enc_write s e d)
| S_AdvWrite h e d :
    Adversarial s h e -> state s e <> INVALID -> pmp s e h = P_RW ->
    step s (do_adv_write s e d).

Inductive reachable : St -> Prop :=
| R_init : reachable init
| R_step s s' : reachable s -> step s s' -> reachable s'.

(** ** Inductive invariant

    Every clause is in prenex form so that the proof automation can
    instantiate it at every enclave and hart in sight.  The properties in
    the form of MODEL.typ are derived from these clauses further below. *)

Definition KeepKind (k : StopKind) : Prop :=
  k = K_share_none \/ k = K_share_ro \/ k = K_invol \/ k = K_exit.

Record Inv (s : St) : Prop := {
  (* structure of run / resume / destroy *)
  i_s1 : forall e h, ctx s h = Enc e \/ ctx s h = Sm e -> runner s e = Some h;
  i_s2 : forall e h, runner s e = Some h ->
           state s e = RUNNING /\
           ((phase s e = IDLE /\ ctx s h = Enc e) \/
            ((phase s e = REVOKING \/ phase s e = VALIDATING) /\
             ctx s h = Sm e));
  i_s3 : forall e, state s e = RUNNING -> runner s e <> None;
  i_s4 : forall e, state s e <> RUNNING ->
           phase s e = IDLE \/ (phase s e = ZEROED /\ state s e = DESTROYING);
  i_s5 : forall e h, ctx s h = Enc e -> share s e = SH_NONE /\ meta s e = None;
  (* permissions *)
  i_a2 : forall e h h2, ctx s h = Enc e ->
           pmp s e h = P_RW /\ (h2 <> h -> pmp s e h2 = P_NONE);
  i_b2 : forall e h, phase s e = VALIDATING -> pmp s e h = P_NONE;
  i_a3 : forall e h, ctx s h <> Enc e -> state s e <> INVALID ->
           (pmp s e h <> P_NONE -> share s e <> SH_NONE) /\
           (pmp s e h = P_RW -> share s e = SH_RW);
  i_d2c : forall e h, state s e = INVALID ->
            share s e = SH_NONE /\ meta s e = None /\ pmp s e h = P_NONE;
  i_a1 : forall e h, state s e = FRESH -> pmp s e h = P_NONE;
  i_a8 : forall e h, state s e = STOPPED ->
           lastStop s e = K_invol \/ lastStop s e = K_exit ->
           share s e = SH_NONE /\ pmp s e h = P_NONE;
  (* confidentiality *)
  i_e1 : forall e h, secret s e = true -> ctx s h <> Enc e ->
           pmp s e h = P_NONE;
  i_z : forall e, phase s e = ZEROED -> secret s e = false;
  (* validation *)
  i_dirty : forall e, dirty s e = true -> share s e = SH_RW;
  i_b5 : forall e, state s e = FRESH ->
           share s e = SH_RW /\ dirty s e = true /\ lastStop s e = K_none;
  i_d2b : forall e, share s e = SH_RW -> state s e <> INVALID ->
            meta s e = sentMeta s e;
  i_b3 : forall e, dead s e = true ->
           state s e = DESTROYING \/ state s e = INVALID;
  (* integrity *)
  i_l : forall e, state s e = STOPPED \/ state s e = RUNNING ->
          KeepKind (lastStop s e) -> share s e <> SH_RW;
  i_e2 : forall e,
           state s e = STOPPED \/ (state s e = RUNNING /\ phase s e <> IDLE) ->
           KeepKind (lastStop s e) -> mem s e = snap s e
}.

(** ** Proof automation *)

Ltac pose_new p :=
  let T := type of p in
  lazymatch goal with
  | _ : T |- _ => fail
  | _ => pose proof p
  end.

(* Instantiate every universally quantified hypothesis (of sort Prop) at
   every enclave and hart in the context. *)
Ltac inst :=
  repeat match goal with
  | H : forall e : EID, _, e0 : EID |- _ =>
      match type of H with
      | ?T => match type of T with Prop => pose_new (H e0) end
      end
  | H : forall h : Hart, _, h0 : Hart |- _ =>
      match type of H with
      | ?T => match type of T with Prop => pose_new (H h0) end
      end
  end.

(* Prove a premise built from equations, disequations, /\ and \/ . *)
Ltac leaf :=
  solve [ assumption | congruence
        | intro; congruence
        | split; leaf
        | left; leaf
        | right; leaf
        | match goal with |- KeepKind _ => unfold KeepKind; leaf end ].

(* Forward chaining over the instantiated hypotheses. *)
Ltac fwd :=
  repeat (try subst; match goal with
  | H : False |- _ => destruct H
  | H : _ /\ _ |- _ => destruct H
  | H : exists _, _ |- _ => destruct H
  | H : _ \/ _ |- _ => destruct H
  | H : ?A -> ?B |- _ =>
      let HA := fresh in
      assert (HA : A) by leaf; specialize (H HA); clear HA
  end).

Ltac cases :=
  repeat match goal with
  | |- context [eid_eq_dec ?a ?b] => destruct (eid_eq_dec a b); try subst
  | |- context [hart_eq_dec ?a ?b] => destruct (hart_eq_dec a b); try subst
  | H : context [eid_eq_dec ?a ?b] |- _ => destruct (eid_eq_dec a b); try subst
  | H : context [hart_eq_dec ?a ?b] |- _ =>
      destruct (hart_eq_dec a b); try subst
  end.

Ltac unfold_all :=
  unfold do_create, do_begin, do_revoke_step, do_revoke_done,
    do_validate_pass, do_validate_fail, do_stop, do_destroy_begin,
    do_destroy_zero, do_destroy_free, do_enc_write, do_adv_write,
    Adversarial, KeepKind, kind_of, HostPerm, iff, not in *;
  cbn [state share meta phase runner ctx pmp mem secret lastStop dirty
       sentMeta dead snap] in *.

Ltac fin :=
  try solve [ congruence | assumption | tauto
            | repeat split; congruence
            | intuition congruence ].

(* [facts] poses the invariant clauses the case analysis needs. *)
Ltac crush facts :=
  (* the step hypothesis mentions the post-state; it is no longer needed
     and would only multiply the case splits below *)
  try match goal with H : step _ _ |- _ => clear H end;
  unfold_all; intros;
  repeat match goal with
  | sh : Share |- _ => destruct sh
  | |- context [match share ?s ?e with _ => _ end] =>
      destruct (share s e) eqn:?
  | H : context [match share ?s ?e with _ => _ end] |- _ =>
      destruct (share s e) eqn:?
  end;
  unfold_all; unfold updE, updH in *; cases;
  facts; inst; fwd; fin.

(** ** The invariant holds initially and is preserved *)

Lemma inv_init : Inv init.
Proof.
  constructor; unfold init, KeepKind; cbn; intros; fin.
Qed.

Section Preservation.

Variables s s' : St.
Hypothesis HI : Inv s.
Hypothesis Hst : step s s'.

Ltac S1 := pose proof (i_s1 s HI).
Ltac S2 := pose proof (i_s2 s HI).
Ltac S3 := pose proof (i_s3 s HI).
Ltac S4 := pose proof (i_s4 s HI).
Ltac S5 := pose proof (i_s5 s HI).
Ltac A2 := pose proof (i_a2 s HI).
Ltac B2 := pose proof (i_b2 s HI).
Ltac A3 := pose proof (i_a3 s HI).
Ltac D2c := pose proof (i_d2c s HI).
Ltac A1 := pose proof (i_a1 s HI).
Ltac A8 := pose proof (i_a8 s HI).
Ltac E1 := pose proof (i_e1 s HI).
Ltac Z := pose proof (i_z s HI).
Ltac Dirty := pose proof (i_dirty s HI).
Ltac B5 := pose proof (i_b5 s HI).
Ltac D2b := pose proof (i_d2b s HI).
Ltac B3 := pose proof (i_b3 s HI).
Ltac L := pose proof (i_l s HI).
Ltac E2 := pose proof (i_e2 s HI).

Lemma p_s1 : forall e h, ctx s' h = Enc e \/ ctx s' h = Sm e -> runner s' e = Some h.
Proof. destruct Hst; crush ltac:(S1; S2). Qed.

Lemma p_s2 : forall e h, runner s' e = Some h ->
  state s' e = RUNNING /\
  ((phase s' e = IDLE /\ ctx s' h = Enc e) \/
   ((phase s' e = REVOKING \/ phase s' e = VALIDATING) /\ ctx s' h = Sm e)).
Proof. destruct Hst; crush ltac:(S1; S2). Qed.

Lemma p_s3 : forall e, state s' e = RUNNING -> runner s' e <> None.
Proof. destruct Hst; crush ltac:(S3). Qed.

Lemma p_s4 : forall e, state s' e <> RUNNING ->
  phase s' e = IDLE \/ (phase s' e = ZEROED /\ state s' e = DESTROYING).
Proof. destruct Hst; crush ltac:(S1; S2; S4). Qed.

Lemma p_s5 : forall e h, ctx s' h = Enc e -> share s' e = SH_NONE /\ meta s' e = None.
Proof. destruct Hst; crush ltac:(S1; S2; S5). Qed.

Lemma p_a2 : forall e h h2, ctx s' h = Enc e ->
  pmp s' e h = P_RW /\ (h2 <> h -> pmp s' e h2 = P_NONE).
Proof. destruct Hst; crush ltac:(S1; S2; A2; B2). Qed.

Lemma p_b2 : forall e h, phase s' e = VALIDATING -> pmp s' e h = P_NONE.
Proof. destruct Hst; crush ltac:(S1; S2; S4; A1; B2). Qed.

Lemma p_a3 : forall e h, ctx s' h <> Enc e -> state s' e <> INVALID ->
  (pmp s' e h <> P_NONE -> share s' e <> SH_NONE) /\
  (pmp s' e h = P_RW -> share s' e = SH_RW).
Proof. destruct Hst; crush ltac:(S1; S2; A2; B2; A3). Qed.

Lemma p_d2c : forall e h, state s' e = INVALID ->
  share s' e = SH_NONE /\ meta s' e = None /\ pmp s' e h = P_NONE.
Proof. destruct Hst; crush ltac:(S1; S2; S4; D2c). Qed.

Lemma p_a1 : forall e h, state s' e = FRESH -> pmp s' e h = P_NONE.
Proof. destruct Hst; crush ltac:(S1; S2; S4; A1). Qed.

Lemma p_a8 : forall e h, state s' e = STOPPED ->
  lastStop s' e = K_invol \/ lastStop s' e = K_exit ->
  share s' e = SH_NONE /\ pmp s' e h = P_NONE.
Proof. destruct Hst; crush ltac:(S1; S2; S4; A2; A8). Qed.

Lemma p_e1 : forall e h, secret s' e = true -> ctx s' h <> Enc e ->
  pmp s' e h = P_NONE.
Proof. destruct Hst; crush ltac:(S1; S2; A2; B2; E1). Qed.

Lemma p_z : forall e, phase s' e = ZEROED -> secret s' e = false.
Proof. destruct Hst; crush ltac:(S1; S2; Z). Qed.

Lemma p_dirty : forall e, dirty s' e = true -> share s' e = SH_RW.
Proof. destruct Hst; crush ltac:(S1; S2; S5; A3; Dirty). Qed.

Lemma p_b5 : forall e, state s' e = FRESH ->
  share s' e = SH_RW /\ dirty s' e = true /\ lastStop s' e = K_none.
Proof. destruct Hst; crush ltac:(S1; S2; S4; B5). Qed.

Lemma p_d2b : forall e, share s' e = SH_RW -> state s' e <> INVALID ->
  meta s' e = sentMeta s' e.
Proof. destruct Hst; crush ltac:(S2; D2b). Qed.

Lemma p_b3 : forall e, dead s' e = true ->
  state s' e = DESTROYING \/ state s' e = INVALID.
Proof. destruct Hst; crush ltac:(S1; S2; B3). Qed.

Lemma p_l : forall e, state s' e = STOPPED \/ state s' e = RUNNING ->
  KeepKind (lastStop s' e) -> share s' e <> SH_RW.
Proof. destruct Hst; crush ltac:(B5; L). Qed.

Lemma p_e2 : forall e,
  state s' e = STOPPED \/ (state s' e = RUNNING /\ phase s' e <> IDLE) ->
  KeepKind (lastStop s' e) -> mem s' e = snap s' e.
Proof. destruct Hst; crush ltac:(S1; S2; A3; B5; L; E2). Qed.

Lemma inv_step : Inv s'.
Proof.
  constructor.
  - exact p_s1. - exact p_s2. - exact p_s3. - exact p_s4. - exact p_s5.
  - exact p_a2. - exact p_b2. - exact p_a3. - exact p_d2c. - exact p_a1.
  - exact p_a8. - exact p_e1. - exact p_z. - exact p_dirty. - exact p_b5.
  - exact p_d2b. - exact p_b3. - exact p_l. - exact p_e2.
Qed.

End Preservation.

Theorem inv_reachable : forall s, reachable s -> Inv s.
Proof.
  induction 1.
  - exact inv_init.
  - eapply inv_step; eassumption.
Qed.

(** ** State properties (MODEL.typ Section 7), for every reachable state

    Names follow ADM.tla.  Where ADM.tla bounds a cardinality or
    quantifies over a finite set, the statement here is the corresponding
    unbounded one. *)

Section StateProperties.

Variable s : St.
Hypothesis R : reachable s.

Let Hinv : Inv s := inv_reachable s R.

Lemma running_of_ctx : forall e h, ctx s h = Enc e \/ ctx s h = Sm e ->
  state s e = RUNNING /\ runner s e = Some h.
Proof.
  intros e h H. pose proof (i_s1 s Hinv e h H) as Hr.
  destruct (i_s2 s Hinv e h Hr). auto.
Qed.

Lemma running_of_phase : forall e,
  phase s e = REVOKING \/ phase s e = VALIDATING -> state s e = RUNNING.
Proof.
  intros e H. destruct (state s e) eqn:E; auto; exfalso;
    destruct (i_s4 s Hinv e) as [|[]]; try congruence; destruct H; congruence.
Qed.

Theorem A1_InitialProtection : forall e h,
  state s e = FRESH -> pmp s e h = P_NONE.
Proof. exact (i_a1 s Hinv). Qed.

Theorem A2_ExclusiveWhileRunning : forall e h,
  ctx s h = Enc e ->
  pmp s e h = P_RW /\ (forall h2, h2 <> h -> pmp s e h2 = P_NONE).
Proof.
  intros e h H. split.
  - exact (proj1 (i_a2 s Hinv e h h H)).
  - intros h2. exact (proj2 (i_a2 s Hinv e h h2 H)).
Qed.

Theorem A3_OnlyExplicitGrant : forall e h,
  ctx s h <> Enc e -> state s e <> INVALID ->
  PermLeq (pmp s e h) (HostPerm (share s e)).
Proof.
  intros e h H1 H2. destruct (i_a3 s Hinv e h H1 H2) as [Ha Hb].
  destruct (pmp s e h), (share s e); cbn; try exact Logic.I; exfalso;
    first [ apply Ha; congruence | pose proof (Hb eq_refl); congruence ].
Qed.

Theorem A6_IsolatedWhenNotShared : forall e h,
  share s e = SH_NONE -> pmp s e h <> P_NONE -> ctx s h = Enc e.
Proof.
  intros e h Hs Hp.
  assert (E : state s e <> INVALID).
  { intro E. destruct (i_d2c s Hinv e h E) as [_ [_ H0]]. contradiction. }
  destruct (ctx s h) as [|e'|e'] eqn:C;
    try (destruct (eid_eq_dec e' e); [subst; reflexivity|]);
    exfalso; refine (proj1 (i_a3 s Hinv e h _ E) Hp Hs); congruence.
Qed.

Theorem A8_InvoluntaryStopSafe : forall e h,
  state s e = STOPPED -> lastStop s e = K_invol \/ lastStop s e = K_exit ->
  share s e = SH_NONE /\ pmp s e h = P_NONE.
Proof. exact (i_a8 s Hinv). Qed.

(** E1 (state form): while the contents are confidential, no adversarial
    hart can access them. *)
Theorem E1_NoUnintendedDisclosure : forall e h,
  secret s e = true -> Adversarial s h e -> pmp s e h = P_NONE.
Proof.
  intros e h Hs [Ha | [e2 [Hne Ha]]]; apply (i_e1 s Hinv e h Hs); congruence.
Qed.

Theorem B1_ValidatedBeforeRun : forall e h,
  ctx s h = Enc e -> dirty s e = false.
Proof.
  intros e h H. destruct (dirty s e) eqn:D; auto.
  pose proof (i_dirty s Hinv e D). destruct (i_s5 s Hinv e h H). congruence.
Qed.

Theorem B2_NoAccessDuringValidation : forall e h,
  phase s e = VALIDATING -> pmp s e h = P_NONE.
Proof. exact (i_b2 s Hinv). Qed.

Theorem B3_FailClosed : forall e,
  dead s e = true ->
  (state s e = DESTROYING \/ state s e = INVALID) /\
  (forall h, ctx s h <> Enc e).
Proof.
  intros e H. pose proof (i_b3 s Hinv e H) as Hd. split; auto.
  intros h C. destruct (running_of_ctx e h (or_introl C)).
  destruct Hd; congruence.
Qed.

Theorem B4_FreshMetadata : forall e,
  phase s e = VALIDATING -> share s e = SH_RW -> meta s e = sentMeta s e.
Proof.
  intros e Hp Hs. apply (i_d2b s Hinv e Hs).
  rewrite (running_of_phase e (or_intror Hp)). discriminate.
Qed.

Theorem B5_FreshNeedsValidation : forall e,
  state s e = FRESH -> share s e = SH_RW /\ dirty s e = true.
Proof. intros e H. destruct (i_b5 s Hinv e H) as [? []]. auto. Qed.

(** E2 (first half): unless the enclave granted write access, the contents
    it finds on resumption are the ones it left. *)
Theorem E2_ContentsPreserved : forall e,
  state s e = STOPPED \/ state s e = RUNNING ->
  KeepKind (lastStop s e) ->
  (forall h, ctx s h <> Enc e) ->
  mem s e = snap s e.
Proof.
  intros e Hst Hk Hn. apply (i_e2 s Hinv e); auto.
  destruct Hst as [|Hr]; auto. right. split; auto.
  destruct (runner s e) as [h|] eqn:Ru.
  - destruct (i_s2 s Hinv e h Ru) as [_ [[_ C] | [Hp _]]].
    + exfalso. exact (Hn h C).
    + destruct Hp; congruence.
  - exfalso. exact (i_s3 s Hinv e Hr Ru).
Qed.

Theorem D2a_RunningConsistent : forall e h,
  ctx s h = Enc e ->
  state s e = RUNNING /\ phase s e = IDLE /\ runner s e = Some h /\
  share s e = SH_NONE /\ meta s e = None.
Proof.
  intros e h H. pose proof (i_s1 s Hinv e h (or_introl H)) as Hr.
  destruct (i_s2 s Hinv e h Hr) as [? [[]|[_ C]]]; [|congruence].
  destruct (i_s5 s Hinv e h H). auto.
Qed.

Theorem D2b_MetaMatchesSent : forall e,
  share s e = SH_RW -> state s e <> INVALID -> meta s e = sentMeta s e.
Proof. exact (i_d2b s Hinv). Qed.

Theorem D2c_InvalidConsistent : forall e h,
  state s e = INVALID ->
  share s e = SH_NONE /\ meta s e = None /\ pmp s e h = P_NONE.
Proof. exact (i_d2c s Hinv). Qed.

(** D2(d): at most one hart executes or enters a given enclave. *)
Theorem D2d_SingleRunner : forall e h1 h2,
  (ctx s h1 = Enc e \/ ctx s h1 = Sm e) ->
  (ctx s h2 = Enc e \/ ctx s h2 = Sm e) -> h1 = h2.
Proof.
  intros e h1 h2 H1 H2.
  pose proof (i_s1 s Hinv e h1 H1). pose proof (i_s1 s Hinv e h2 H2). congruence.
Qed.

End StateProperties.

(** ** Step properties (the action properties of ADM.tla) *)

Section StepProperties.

Variables s s' : St.
Hypothesis R : reachable s.
Hypothesis Hst : step s s'.

Let Hinv : Inv s := inv_reachable s R.

(* Goal solver for the step properties: chooses disjuncts and witnesses. *)
Ltac goal_solve :=
  solve [ congruence | assumption
        | split; goal_solve
        | left; goal_solve
        | right; goal_solve
        | match goal with
          | h : Hart |- exists _ : Hart, _ =>
              exists h; unfold updH; cases; goal_solve
          end ].

Ltac act facts :=
  destruct Hst; clear Hst; unfold_all; intros;
  try match goal with |- _ /\ _ => split; intros end;
  repeat match goal with sh : Share |- _ => destruct sh end;
  unfold updE, updH in *; cases;
  facts; inst; fwd; try goal_solve; fin.

Ltac S1 := pose proof (i_s1 s Hinv).
Ltac S2 := pose proof (i_s2 s Hinv).
Ltac B2 := pose proof (i_b2 s Hinv).
Ltac D2b := pose proof (i_d2b s Hinv).
Ltac Z := pose proof (i_z s Hinv).

(** A4: the enclave is entered only when no hart holds a permission. *)
Theorem A4_RevokedBeforeEnter : forall e h,
  ctx s h <> Enc e -> ctx s' h = Enc e ->
  forall h2, pmp s e h2 = P_NONE.
Proof. act ltac:(B2). Qed.

(** A7: the sharing state and the metadata change only through Create, a
    stop issued by the owner, the SM's validation, or destruction. *)
Theorem A7_ShareStateIntegrity : forall e,
  share s' e <> share s e \/ meta s' e <> meta s e ->
  (state s e = INVALID /\ state s' e = FRESH) \/
  (exists h, ctx s h = Enc e /\ ctx s' h = Host) \/
  (phase s e = VALIDATING /\
   exists h, ctx s h = Sm e /\ ctx s' h = Enc e) \/
  (state s e = DESTROYING /\ state s' e = INVALID).
Proof. act ltac:(S1; S2). Qed.

(** A9: when the region is released, it holds nothing the enclave keeps
    confidential. *)
Theorem A9_NoSecretAtRelease : forall e,
  state s e = DESTROYING -> state s' e = INVALID -> secret s e = false.
Proof. act ltac:(Z). Qed.

(** B1 / B5 (step form): the enclave is entered only out of the validation
    phase, and after a writable share only with well-formed contents with
    respect to the legitimately supplied metadata. *)
Theorem B5_EnterOnlyViaValidation : forall e h,
  ctx s h <> Enc e -> ctx s' h = Enc e ->
  phase s e = VALIDATING /\
  (share s e = SH_RW -> WFo (mem s e) (sentMeta s e)).
Proof.
  act ltac:(S1; S2; D2b);
    (* ValidatePass: rewrite the stored metadata into the supplied one *)
    match goal with
    | Hw : WFo ?d (meta s ?e), Hm : meta s ?e = sentMeta s ?e
      |- WFo ?d (sentMeta s ?e) => rewrite <- Hm; exact Hw
    end.
Qed.

(** D1: an enclave is stopped only by itself, and started only by the
    host.  (Create and DestroyBegin are host-only by their guards.) *)
Theorem D1_CallerRestriction : forall e,
  (state s e = RUNNING -> state s' e = STOPPED ->
   exists h, ctx s h = Enc e /\ ctx s' h = Host) /\
  (state s e = FRESH \/ state s e = STOPPED -> state s' e = RUNNING ->
   exists h, ctx s h = Host /\ ctx s' h = Sm e).
Proof. act ltac:(S1; S2). Qed.

End StepProperties.

End ADM.
