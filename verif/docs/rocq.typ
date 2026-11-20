// Appendix A of MODEL.typ and MODEL.en.typ, and Section 2 of PROOF.typ and
// PROOF.en.typ (with full: true): the definitions and theorems of
// verif/rocq/ADM.v and ADMRegions.v as formulas.  The formulas are shared;
// only the prose depends on the language.

// fields of a state
#let st = math.op("state")
#let sh = math.op("share")
#let mt = math.op("meta")
#let ph = math.op("phase")
#let rn = math.op("runner")
#let cx = math.op("ctx")
#let pm = math.op("pmp")
#let mm = math.op("mem")
#let sec = math.op("secret")
#let ls = math.op("lastStop")
#let dt = math.op("dirty")
#let sm = math.op("sentMeta")
#let dd = math.op("dead")
#let sn = math.op("snap")
#let epm = math.op("epm")
#let adm = math.op("adm")
// derived notions
#let hp = math.op("hostPerm")
#let Adv = math.op("Adv")
#let Keep = math.op("Keep")
#let WFo = math.op("WF")
#let Reach = math.op("Reach")
#let ReachR = $Reach_"R"$
#let toR = $attach(->, br: "R")$
#let Inv = math.op("Inv")
#let Prot = math.op("Prot")
#let MP = math.op("Match")
#let EP = math.op("EpmPerm")
#let Enc = math.op("Enc")
#let Sm = math.op("Sm")
#let SMR = math.op("SMRegion")
// constants
#let c(s) = math.upright(text(size: 0.85em, s))
#let l(s) = math.upright(s)
#let Host = c("HOST")
#let INVALID = c("INVALID")
#let FRESH = c("FRESH")
#let RUNNING = c("RUNNING")
#let STOPPED = c("STOPPED")
#let DESTROYING = c("DESTROYING")
#let IDLE = c("IDLE")
#let REVOKING = c("REVOKING")
#let VALIDATING = c("VALIDATING")
#let ZEROED = c("ZEROED")
#let NONE = c("NONE")
#let RONLY = c("RONLY")
#let Rd = c("R")
#let RW = c("RW")
#let ALL = c("ALL")
#let tt = l("true")
#let ff = l("false")
#let invol = l("invol")
#let exit = l("exit")
#let knone = l("none")
#let sharenone = l("share_none")
#let sharero = l("share_ro")
#let sharerw = l("share_rw")

#let rocq-appendix(lang, prefix: "A", title: auto, full: false) = {
  let t(ja, en) = if lang == "ja" { ja } else { en }
  let gloss(..rows) = table(
    columns: (2fr, 6fr, 14fr),
    table.header([*ID*], [*Rocq*], [*#t("意味", "Meaning")*]),
    ..rows,
  )
  let rules(..rows) = table(
    columns: (3fr, 7fr, 10fr),
    table.header(
      [*#t("遷移", "Transition")*], [*#t("前提", "Premises")*], [*#t("遷移後の状態", "Post-state")*],
    ),
    ..rows,
  )
  let thms(..rows) = table(
    columns: (2fr, 15fr),
    table.header([*ID*], [*#t("定理", "Theorem")*]),
    ..rows,
  )

  [
    = #(if title == auto { t("付録 A. Rocq で証明した定理", "Appendix A. Theorems proved in Rocq") } else { title })

    #t(
      [`verif/rocq/ADM.v` と `ADMRegions.v` の定義と定理を数式で書いたもの。名前は Rocq のものと同じである。],
      [The definitions and theorems of `verif/rocq/ADM.v` and `ADMRegions.v` as formulas. The names are those of the Rocq text.],
    )

    == #(prefix + t(".1 状態", ".1 States"))

    #t(
      [$H$（hart）、$E$（エンクレーブ ID）、$D$（ADM の内容）、$M$（メタデータ）は任意の型で、$H$ と $E$ は等号が決定可能とする。$0 in D$ はゼロ化した内容、$WFo subset.eq D times M$ は任意の述語である。$bot$ は「なし」を表す。],
      [$H$ (harts), $E$ (enclave IDs), $D$ (ADM contents) and $M$ (metadata) are arbitrary types; equality on $H$ and $E$ is decidable. $0 in D$ is the zeroed contents, and $WFo subset.eq D times M$ is an arbitrary predicate. $bot$ stands for "none".],
    )

    #t([状態 $sigma$ は次の関数の組である。], [A state $sigma$ is a tuple of the following functions.])

    $
      st &: E -> {INVALID, FRESH, RUNNING, STOPPED, DESTROYING} \
      sh &: E -> {NONE, RONLY, RW} \
      mt, sm &: E -> M union {bot} \
      ph &: E -> {IDLE, REVOKING, VALIDATING, ZEROED} \
      rn &: E -> H union {bot} \
      cx &: H -> {Host} union {Enc(e), Sm(e) | e in E} \
      pm &: E times H -> {NONE, Rd, RW} \
      mm, sn &: E -> D \
      sec, dt, dd &: E -> {tt, ff} \
      ls &: E -> {knone, sharenone, sharero, sharerw, invol, exit}
    $


    #if full [
      #t(
        [各関数の意味は次のとおり。\* をつけたものは証明のための補助変数で、SM の実装に対応する値はない。],
        [The functions mean the following. Those marked with \* are auxiliary variables of the proof, with no counterpart in the implementation of the SM.],
      )
      #table(
        columns: (2fr, 12fr),
        table.header([*#t("関数", "Function")*], [*#t("意味", "Meaning")*]),
        [$st(e)$], [#t([エンクレーブ $e$ のライフサイクルの状態], [the lifecycle state of enclave $e$])],
        [$sh(e)$], [#t([共有状態（$e$ が ADM についてホストに与えている権限）], [the sharing state (the permission $e$ grants the host on its ADM)])],
        [$mt(e)$], [#t([SM が検証に使うメタデータ], [the metadata the SM validates against])],
        [$ph(e)$], [#t([SM の処理の進行（権限の回収中、検証中、ゼロ化済み）], [the progress inside the SM (revoking, validating, zeroed)])],
        [$rn(e)$], [#t([$e$ を実行中、または $e$ に入る処理中の hart], [the hart that executes $e$ or is entering it])],
        [$cx(h)$], [#t([hart $h$ の文脈：ホスト、エンクレーブ $e$ の実行（$Enc(e)$）、$e$ に入るための SM の処理（$Sm(e)$）], [the context of hart $h$: the host, executing enclave $e$ ($Enc(e)$), or the SM entering $e$ ($Sm(e)$)])],
        [$pm(e, h)$], [#t([hart $h$ が S/U モードで $e$ の ADM に持つ権限（PMP の設定）], [the permission of hart $h$ in S/U mode on the ADM of $e$ (the PMP setting)])],
        [$mm(e)$], [#t([$e$ の ADM の内容], [the contents of the ADM of $e$])],
        [$sec(e)$ \*], [#t([内容が、$e$ が開示していない秘密である], [the contents are a secret that $e$ has not disclosed])],
        [$ls(e)$ \*], [#t([直近の停止の種類（共有つきの停止、割り込みによる停止、exit）], [how $e$ stopped most recently (stop with a share, involuntary stop, exit)])],
        [$dt(e)$ \*], [#t([$e$ 以外が書けた可能性があり、まだ検証されていない], [someone other than $e$ could have written, and it is not validated yet])],
        [$sm(e)$ \*], [#t([正当な出所（作成時、または $e$ 自身の停止）から渡されたメタデータ], [the metadata from a legitimate source (creation, or a stop by $e$ itself)])],
        [$dd(e)$ \*], [#t([検証に失敗した], [validation failed])],
        [$sn(e)$ \*], [#t([直近の停止時の内容], [the contents at the most recent stop])],
      )
    ]

    #t(
      [必要なときは $st_sigma (e)$ のように状態を添える。補助の定義は次のとおり。],
      [Where needed, the state is written as a subscript, as in $st_sigma (e)$. Auxiliary definitions:],
    )

    $
      hp(NONE) = NONE, quad hp(RONLY) = Rd, quad hp(RW) = RW, quad quad NONE < Rd < RW \
      Adv(h, e) <=> cx(h) = Host or (exists e' != e. thick cx(h) = Enc(e')) \
      Keep(k) <=> k in {sharenone, sharero, invol, exit} \
      WFo(d, bot) "is false"
    $

    #t(
      [初期状態 $sigma_0$ は、すべての $e$, $h$ について次を満たす。],
      [The initial state $sigma_0$ satisfies, for every $e$ and $h$:],
    )

    $
      st(e) = INVALID, quad sh(e) = NONE, quad mt(e) = sm(e) = rn(e) = bot, quad ph(e) = IDLE, \
      cx(h) = Host, quad pm(e, h) = NONE, quad mm(e) = sn(e) = 0, \
      sec(e) = dt(e) = dd(e) = ff, quad ls(e) = knone
    $

    == #(prefix + t(".2 遷移関係", ".2 Transition relation"))

    #t(
      [$sigma -> sigma'$ は、次の規則のどれかで $sigma'$ が $sigma$ から得られることである。プライムなしは $sigma$、プライムつきは $sigma'$ の値で、書かれていない値は変わらない。$h, h' in H$、$e in E$、$d in D$、$m_0 in M$、$m in M union {bot}$ は任意である。],
      [$sigma -> sigma'$ holds when $sigma'$ is obtained from $sigma$ by one of the rules below. Unprimed values are those of $sigma$ and primed ones those of $sigma'$; a value that is not mentioned is unchanged. $h, h' in H$, $e in E$, $d in D$, $m_0 in M$ and $m in M union {bot}$ are arbitrary.],
    )

    #rules(
      [Create],
      [$cx(h) = Host$, $st(e) = INVALID$],
      [$st'(e) = FRESH$, $sh'(e) = RW$, $mt'(e) = sm'(e) = m_0$, $forall h'. thick pm'(e, h') = NONE$, $mm'(e) = d$, $sec'(e) = ff$, $dt'(e) = tt$, $dd'(e) = ff$, $ls'(e) = knone$],
      [RunBegin],
      [$cx(h) = Host$, $st(e) = FRESH$],
      [$st'(e) = RUNNING$, $ph'(e) = VALIDATING$, $rn'(e) = h$, $cx'(h) = Sm(e)$],
      [ResumeBegin],
      [$cx(h) = Host$, $st(e) = STOPPED$, $ph(e) = IDLE$],
      [$st'(e) = RUNNING$, $ph'(e) = REVOKING$, $rn'(e) = h$, $cx'(h) = Sm(e)$],
      [RevokeStep],
      [$ph(e) = REVOKING$, $pm(e, h) != NONE$],
      [$pm'(e, h) = NONE$],
      [RevokeDone],
      [$ph(e) = REVOKING$, $forall h'. thick pm(e, h') = NONE$],
      [$ph'(e) = VALIDATING$],
      [ValidatePass],
      [$ph(e) = VALIDATING$, $rn(e) = h$, $sh(e) != RW or WFo(mm(e), mt(e))$],
      [$sh'(e) = NONE$, $mt'(e) = bot$, $ph'(e) = IDLE$, $cx'(h) = Enc(e)$, $pm'(e, h) = RW$; $dt'(e) = ff$ #t("（", "(if ")$sh(e) = RW$#t(" のとき）", ")")],
      [ValidateFail],
      [$ph(e) = VALIDATING$, $rn(e) = h$, $sh(e) = RW$, $not WFo(mm(e), mt(e))$],
      [$st'(e) = DESTROYING$, $ph'(e) = IDLE$, $rn'(e) = bot$, $cx'(h) = Host$, $dd'(e) = tt$],
      [Stop$(s, m, k)$],
      [$cx(h) = Enc(e)$, $st(e) = RUNNING$],
      [$st'(e) = STOPPED$, $sh'(e) = s$, $mt'(e) = m$, $rn'(e) = bot$, $cx'(h) = Host$, $ls'(e) = k$, $sn'(e) = mm(e)$; \
        $s = NONE$: $pm'(e, h) = NONE$; \
        $s != NONE$: $forall h'. thick pm'(e, h') = hp(s)$, $sec'(e) = ff$; \
        $s = RW$: $dt'(e) = tt$, $sm'(e) = m$],
      [StopShare],
      [Stop$(s, m, k)$ #t("で", "with") $s = RW <=> m != bot$, $k = sharenone, sharero, sharerw$ #t("（", "(for ")$s = NONE, RONLY, RW$#t("）", ")")],
      [],
      [StopInvoluntary, Exit],
      [Stop$(NONE, bot, invol)$, Stop$(NONE, bot, exit)$],
      [],
      [DestroyBegin],
      [$cx(h) = Host$, $st(e) in {FRESH, STOPPED}$, $ph(e) = IDLE$],
      [$st'(e) = DESTROYING$],
      [DestroyZero],
      [$st(e) = DESTROYING$, $ph(e) = IDLE$],
      [$mm'(e) = 0$, $sec'(e) = ff$, $ph'(e) = ZEROED$],
      [DestroyFree],
      [$st(e) = DESTROYING$, $ph(e) = ZEROED$],
      [$st'(e) = INVALID$, $sh'(e) = NONE$, $mt'(e) = sm'(e) = bot$, $ph'(e) = IDLE$, $forall h'. thick pm'(e, h') = NONE$, $ls'(e) = knone$, $dt'(e) = ff$],
      [EncWrite],
      [$cx(h) = Enc(e)$, $pm(e, h) = RW$],
      [$mm'(e) = d$, $sec'(e) = tt$],
      [AdvWrite],
      [$Adv(h, e)$, $st(e) != INVALID$, $pm(e, h) = RW$],
      [$mm'(e) = d$, $sec'(e) = ff$, $dt'(e) = tt$],
    )


    #if full [
      #t(
        [Create はホストによる作成、RunBegin と ResumeBegin はホストによる実行と再開の要求である。SM は RevokeStep で各 hart の権限を回収し、RevokeDone で回収を終え、ValidatePass か ValidateFail で検証の結果に従う。Stop はエンクレーブ自身による停止（共有つき、割り込み、exit）、DestroyBegin、DestroyZero、DestroyFree は破棄、EncWrite と AdvWrite は、エンクレーブ、または敵対者（ホストか他のエンクレーブ）による書き込みである。],
        [Create is the creation by the host, and RunBegin and ResumeBegin are its requests to run and to resume. The SM revokes the permission of each hart with RevokeStep, ends the revocation with RevokeDone, and follows the result of the validation with ValidatePass or ValidateFail. Stop is a stop by the enclave itself (with a share, involuntary, or exit); DestroyBegin, DestroyZero and DestroyFree destroy the enclave; EncWrite and AdvWrite are writes by the enclave and by the adversary (the host or another enclave).],
      )
    ]

    #t(
      [到達可能な状態の集合 $Reach$ は、$sigma_0 in Reach$ と「$sigma in Reach$ かつ $sigma -> sigma'$ ならば $sigma' in Reach$」を満たす最小の集合である。],
      [The set $Reach$ of reachable states is the least set such that $sigma_0 in Reach$, and $sigma' in Reach$ whenever $sigma in Reach$ and $sigma -> sigma'$.],
    )

    == #(prefix + t(".3 帰納的不変条件", ".3 Inductive invariant"))

    #t(
      [$Inv(sigma)$ は、すべての $e in E$ と $h, h' in H$ について次の19条件が成り立つことである。],
      [$Inv(sigma)$ holds when the following 19 conditions hold for every $e in E$ and $h, h' in H$.],
    )

    #thms(
      [S1], [$cx(h) in {Enc(e), Sm(e)} ==> rn(e) = h$],
      [S2], [$rn(e) = h ==> st(e) = RUNNING and ((ph(e) = IDLE and cx(h) = Enc(e)) or (ph(e) in {REVOKING, VALIDATING} and cx(h) = Sm(e)))$],
      [S3], [$st(e) = RUNNING ==> rn(e) != bot$],
      [S4], [$st(e) != RUNNING ==> ph(e) = IDLE or (ph(e) = ZEROED and st(e) = DESTROYING)$],
      [S5], [$cx(h) = Enc(e) ==> sh(e) = NONE and mt(e) = bot$],
      [A2], [$cx(h) = Enc(e) ==> pm(e, h) = RW and (h' != h ==> pm(e, h') = NONE)$],
      [B2], [$ph(e) = VALIDATING ==> pm(e, h) = NONE$],
      [A3], [$cx(h) != Enc(e) and st(e) != INVALID ==> (pm(e, h) != NONE ==> sh(e) != NONE) and (pm(e, h) = RW ==> sh(e) = RW)$],
      [D2c], [$st(e) = INVALID ==> sh(e) = NONE and mt(e) = bot and pm(e, h) = NONE$],
      [A1], [$st(e) = FRESH ==> pm(e, h) = NONE$],
      [A8], [$st(e) = STOPPED and ls(e) in {invol, exit} ==> sh(e) = NONE and pm(e, h) = NONE$],
      [E1], [$sec(e) and cx(h) != Enc(e) ==> pm(e, h) = NONE$],
      [Z], [$ph(e) = ZEROED ==> not sec(e)$],
      [Dirty], [$dt(e) ==> sh(e) = RW$],
      [B5], [$st(e) = FRESH ==> sh(e) = RW and dt(e) and ls(e) = knone$],
      [D2b], [$sh(e) = RW and st(e) != INVALID ==> mt(e) = sm(e)$],
      [B3], [$dd(e) ==> st(e) in {DESTROYING, INVALID}$],
      [L], [$st(e) in {STOPPED, RUNNING} and Keep(ls(e)) ==> sh(e) != RW$],
      [E2], [$(st(e) = STOPPED or (st(e) = RUNNING and ph(e) != IDLE)) and Keep(ls(e)) ==> mm(e) = sn(e)$],
    )

    #t([*定理*（`inv_init`, `inv_step`, `inv_reachable`）], [*Theorem* (`inv_init`, `inv_step`, `inv_reachable`)])

    $
      Inv(sigma_0), quad quad Inv(sigma) and sigma -> sigma' ==> Inv(sigma'), quad quad forall sigma in Reach. thick Inv(sigma)
    $

    == #(prefix + t(".4 状態の性質", ".4 State properties"))


    #if full {
      gloss(
        [A1], [`A1_InitialProtection`], t([作成直後は、どの hart も ADM にアクセスできない], [right after creation, no hart can access the ADM]),
        [A2], [`A2_ExclusiveWhileRunning`], t([実行中は、実行している hart だけが読み書きでき、他の hart はアクセスできない], [while the enclave runs, only the hart that runs it can read and write; no other hart has access]),
        [A3], [`A3_OnlyExplicitGrant`], t([エンクレーブの外の hart の権限は、共有状態が許す範囲を超えない], [a hart outside the enclave has no more permission than the sharing state grants]),
        [A6], [`A6_IsolatedWhenNotShared`], t([共有していないとき、権限を持つのはそのエンクレーブを実行中の hart だけである], [when the ADM is not shared, only the hart that runs the enclave has a permission]),
        [A8], [`A8_InvoluntaryStopSafe`], t([割り込みによる停止と exit の後は、共有されず、どの hart もアクセスできない], [after an involuntary stop or an exit, the ADM is not shared and no hart has access]),
        [E1], [`E1_NoUnintendedDisclosure`], t([秘密の内容には、敵対的な hart がアクセスできない], [no adversarial hart can access secret contents]),
        [B1], [`B1_ValidatedBeforeRun`], t([実行中のエンクレーブの ADM は、検証されていない書き込みを含まない], [the ADM of a running enclave holds no write that was not validated]),
        [B2, B8], [`B2_NoAccessDuringValidation`], t([検証中は、どの hart もアクセスできない], [during validation, no hart has access]),
        [B3], [`B3_FailClosed`], t([検証に失敗したエンクレーブは破棄に向かい、実行されない], [an enclave that failed validation is being destroyed and does not run]),
        [B4], [`B4_FreshMetadata`], t([検証に使うメタデータは、正当な出所から渡されたものである], [the metadata used for validation comes from a legitimate source]),
        [B5], [`B5_FreshNeedsValidation`], t([作成直後の ADM は、書き込み可能な共有の後と同じく検証を要する], [a freshly created ADM needs validation, as after a writable share]),
        [E2], [`E2_ContentsPreserved`], t([書き込みを許していなければ、再開時の内容は停止時のままである], [unless the enclave granted write access, the contents on resumption are those it left]),
        [D2a], [`D2a_RunningConsistent`], t([実行中のエンクレーブの状態は整合している], [the state of a running enclave is consistent]),
        [D2b], [`D2b_MetaMatchesSent`], t([書き込み可能な共有の間、SM が持つメタデータは渡されたものと一致する], [during a writable share, the metadata held by the SM is the one supplied]),
        [D2c], [`D2c_InvalidConsistent`], t([存在しないエンクレーブには、共有状態もメタデータも権限もない], [an enclave that does not exist has no sharing state, metadata or permission]),
        [D2d], [`D2d_SingleRunner`], t([同じエンクレーブを実行中または進入中の hart は高々1つである], [at most one hart executes or enters a given enclave]),
      )
    }

    #t(
      [すべての $sigma in Reach$、$e in E$、$h, h_1, h_2 in H$ について次が成り立つ。],
      [For every $sigma in Reach$, $e in E$ and $h, h_1, h_2 in H$:],
    )

    #thms(
      [A1], [$st(e) = FRESH ==> pm(e, h) = NONE$],
      [A2], [$cx(h) = Enc(e) ==> pm(e, h) = RW and forall h' != h. thick pm(e, h') = NONE$],
      [A3], [$cx(h) != Enc(e) and st(e) != INVALID ==> pm(e, h) <= hp(sh(e))$],
      [A6], [$sh(e) = NONE and pm(e, h) != NONE ==> cx(h) = Enc(e)$],
      [A8], [$st(e) = STOPPED and ls(e) in {invol, exit} ==> sh(e) = NONE and pm(e, h) = NONE$],
      [E1], [$sec(e) and Adv(h, e) ==> pm(e, h) = NONE$],
      [B1], [$cx(h) = Enc(e) ==> not dt(e)$],
      [B2, B8], [$ph(e) = VALIDATING ==> pm(e, h) = NONE$],
      [B3], [$dd(e) ==> st(e) in {DESTROYING, INVALID} and forall h'. thick cx(h') != Enc(e)$],
      [B4], [$ph(e) = VALIDATING and sh(e) = RW ==> mt(e) = sm(e)$],
      [B5], [$st(e) = FRESH ==> sh(e) = RW and dt(e)$],
      [E2], [$st(e) in {STOPPED, RUNNING} and Keep(ls(e)) and (forall h'. thick cx(h') != Enc(e)) ==> mm(e) = sn(e)$],
      [D2a], [$cx(h) = Enc(e) ==> st(e) = RUNNING and ph(e) = IDLE and rn(e) = h and sh(e) = NONE and mt(e) = bot$],
      [D2b], [$sh(e) = RW and st(e) != INVALID ==> mt(e) = sm(e)$],
      [D2c], [$st(e) = INVALID ==> sh(e) = NONE and mt(e) = bot and pm(e, h) = NONE$],
      [D2d], [$cx(h_1) in {Enc(e), Sm(e)} and cx(h_2) in {Enc(e), Sm(e)} ==> h_1 = h_2$],
    )

    == #(prefix + t(".5 遷移の性質", ".5 Step properties"))


    #if full {
      gloss(
        [A4], [`A4_RevokedBeforeEnter`], t([エンクレーブに入る時点で、どの hart も権限を持たない], [when the enclave is entered, no hart holds a permission]),
        [A7], [`A7_ShareStateIntegrity`], t([共有状態とメタデータを変えるのは、作成、所有者による停止、SM の検証、破棄だけである], [the sharing state and the metadata change only by creation, a stop by the owner, the validation of the SM, or destruction]),
        [A9], [`A9_NoSecretAtRelease`], t([領域を解放するとき、秘密の内容は残っていない], [when the region is released, it holds no secret]),
        [B5], [`B5_EnterOnlyViaValidation`], t([エンクレーブに入るのは検証の後だけで、書き込み可能な共有の後は内容が整形式である], [the enclave is entered only after validation, and after a writable share only with well-formed contents]),
        [D1], [`D1_CallerRestriction`], t([エンクレーブを停止するのはそれ自身だけ、開始するのはホストだけである], [an enclave is stopped only by itself and started only by the host]),
      )
    }

    #t(
      [すべての $sigma in Reach$、$sigma -> sigma'$、$e in E$、$h in H$ について次が成り立つ。],
      [For every $sigma in Reach$, $sigma -> sigma'$, $e in E$ and $h in H$:],
    )

    #thms(
      [A4], [$cx_sigma (h) != Enc(e) and cx_(sigma') (h) = Enc(e) ==> forall h'. thick pm_sigma (e, h') = NONE$],
      [A7], [$sh_(sigma') (e) != sh_sigma (e) or mt_(sigma') (e) != mt_sigma (e) ==>$ \
        $quad (st_sigma (e) = INVALID and st_(sigma') (e) = FRESH)$ \
        $quad or (exists h'. thick cx_sigma (h') = Enc(e) and cx_(sigma') (h') = Host)$ \
        $quad or (ph_sigma (e) = VALIDATING and exists h'. thick cx_sigma (h') = Sm(e) and cx_(sigma') (h') = Enc(e))$ \
        $quad or (st_sigma (e) = DESTROYING and st_(sigma') (e) = INVALID)$],
      [A9], [$st_sigma (e) = DESTROYING and st_(sigma') (e) = INVALID ==> not sec_sigma (e)$],
      [B5], [$cx_sigma (h) != Enc(e) and cx_(sigma') (h) = Enc(e) ==> ph_sigma (e) = VALIDATING and (sh_sigma (e) = RW ==> WFo(mm_sigma (e), sm_sigma (e)))$],
      [D1], [$(st_sigma (e) = RUNNING and st_(sigma') (e) = STOPPED ==> exists h'. thick cx_sigma (h') = Enc(e) and cx_(sigma') (h') = Host)$ \
        $and (st_sigma (e) in {FRESH, STOPPED} and st_(sigma') (e) = RUNNING ==> exists h'. thick cx_sigma (h') = Host and cx_(sigma') (h') = Sm(e))$],
    )

    == #(prefix + t(".6 領域の配置（C）", ".6 Region placement (C)"))

    #t(
      [$A$（アドレス）は任意の型、$SMR subset.eq A$ は SM の領域である。配置 $rho$ は $epm, adm : E -> cal(P)(A)$ の組で、$Prot(e) = epm(e) union adm(e)$ と書く。$(sigma, rho) toR (sigma', rho')$ は次のどれかである。],
      [$A$ (addresses) is an arbitrary type, and $SMR subset.eq A$ is the region of the SM. A placement $rho$ is a pair $epm, adm : E -> cal(P)(A)$, with $Prot(e) = epm(e) union adm(e)$. $(sigma, rho) toR (sigma', rho')$ is one of the following.],
    )

    - #t([Create で], [a Create with]) $epm'(e) = R_e$, $adm'(e) = R_a$#t("。ただし", ", where") $R_e, R_a != emptyset$, $R_e inter R_a = emptyset$, $(R_e union R_a) inter SMR = emptyset$, $forall e_2 != e. thick (R_e union R_a) inter Prot(e_2) = emptyset$#t("。", ";")
    - #t([DestroyFree で], [a DestroyFree with]) $epm'(e) = adm'(e) = emptyset$#t("。", ";")
    - #t([それ以外の遷移 $sigma -> sigma'$ で], [any other step $sigma -> sigma'$ with]) $rho' = rho$#t(
        [（`step_cases`：到達可能な状態からの遷移は、Create、DestroyFree、または $st(e) = INVALID$ である $e$ の集合を変えない遷移のどれかである）。],
        [ (`step_cases`: a step from a reachable state is a Create, a DestroyFree, or a step that keeps the set of $e$ with $st(e) = INVALID$).],
      )

    #t(
      [$ReachR$ は、$(sigma_0, rho_0)$（$rho_0$ はすべて空）から $toR$ で到達できる組の集合である。hart $h$ でアドレス $a$ にマッチするエンクレーブ用エントリの権限の集合を次のように定める。],
      [$ReachR$ is the set of pairs reachable from $(sigma_0, rho_0)$ ($rho_0$ all empty) by $toR$. The set of permissions of the enclave entries of hart $h$ that match address $a$ is],
    )

    $
      MP(h, a) &= {EP(h, e) | a in epm(e)} union {pm(e, h) | a in adm(e)} \
      EP(h, e) &= cases(ALL & quad "if" thick cx(h) = Enc(e), NONE & quad "otherwise")
    $


    #if full {
      gloss(
        [C1], [`C1_RegionsDisjoint_holds`], t([各エンクレーブの EPM と ADM は、互いにも、SM とも、他のエンクレーブの領域とも交わらない], [the EPM and the ADM of an enclave meet neither each other, the SM, nor a region of another enclave]),
        [—], [`C_RegionsFollowLifecycle_holds`], t([領域があるのは、エンクレーブが存在する間だけである], [the regions exist exactly while the enclave exists]),
        [C2], [`C2_SMNotShadowed`], t([SM の領域は、エンクレーブ用のどのエントリにもマッチしない], [the region of the SM is matched by no enclave entry]),
        [—], [`C_EPMIsolated`], t([EPM のページにマッチするのは、そのエンクレーブの EPM のエントリだけである], [a page of an EPM is matched only by the EPM entry of its enclave]),
        [—], [`C_ADMPermFaithful`], t([ADM のページに適用される権限は、エントリの優先順位によらず $pm(e, h)$ である], [the permission that applies to a page of an ADM is $pm(e, h)$, whatever the priority of the entries]),
      )
    }

    #t(
      [すべての $(sigma, rho) in ReachR$、$e, e_1, e_2 in E$、$h in H$、$a in A$ について次が成り立つ。],
      [For every $(sigma, rho) in ReachR$, $e, e_1, e_2 in E$, $h in H$ and $a in A$:],
    )

    #thms(
      [—], [$sigma in Reach$ #t([（したがって #(prefix + ".3")〜#(prefix + ".5") がすべて成り立つ）], [(so everything of #(prefix + ".3")–#(prefix + ".5") holds)])],
      [C1], [$epm(e) inter adm(e) = emptyset and Prot(e) inter SMR = emptyset and (e_1 != e_2 ==> Prot(e_1) inter Prot(e_2) = emptyset)$],
      [—], [$st(e) = INVALID <==> epm(e) = emptyset and adm(e) = emptyset$],
      [C2], [$a in SMR ==> MP(h, a) = emptyset$],
      [—], [$a in epm(e) ==> MP(h, a) = {EP(h, e)}$],
      [—], [$a in adm(e) ==> MP(h, a) = {pm(e, h)}$],
    )

    #t(
      [どの定理も公理に依存しない（`Print Assumptions` が "Closed under the global context" を返す）。],
      [No theorem depends on an axiom (`Print Assumptions` reports "Closed under the global context").],
    )
  ]
}
