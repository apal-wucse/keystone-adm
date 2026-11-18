DOCS := MODEL MODEL.en
PDFS := $(addsuffix .pdf,$(DOCS))

ASCIIDOCTOR_PDF ?= asciidoctor-pdf
# Directory containing ipaexm.ttf; set by the verif devshell
VERIF_FONTS_DIR ?= $(error VERIF_FONTS_DIR is not set; enter the verif devshell)

ADOC_FLAGS := --failure-level WARN \
	-a pdf-themesdir=docs/theme \
	-a pdf-theme=model \
	-a pdf-fontsdir="$(VERIF_FONTS_DIR);GEM_FONTS_DIR"

.PHONY: all docs check reach prove cbmc clean
all: docs

docs: $(PDFS)

%.pdf: docs/%.adoc docs/theme/model-theme.yml
	$(ASCIIDOCTOR_PDF) $(ADOC_FLAGS) -o $@ $<

# --- TLA+ model checking -----------------------------------------------------

TLC ?= tlc
TLC_FLAGS := -deadlock -workers auto
TLC_META := $(CURDIR)/tla/.tlc

REACH := Reach_ResumedAfterRWShare Reach_TwoEnclavesRunning \
	Reach_SharedVisibleToOtherEnclave Reach_PartialRevocation \
	Reach_ValidationFails
REACH_REGIONS := Reach_NonNapotADM

# Design model: both runs must report no error.
#   ADM.cfg        groups A, B, D, E (regions abstracted away)
#   ADMRegions.cfg group C (region placement and PMP entry priority)
check:
	cd tla && $(TLC) $(TLC_FLAGS) -metadir $(TLC_META)/check -config ADM.cfg ADM.tla
	cd tla && $(TLC) $(TLC_FLAGS) -metadir $(TLC_META)/check-regions \
		-config ADMRegions.cfg ADMRegions.tla

# Reachability witnesses: every invariant must be reported as violated.
# One TLC run per witness, each stopping at its first violation.
# $(1) = module, $(2) = config holding the constants, $(3) = witness names
define run_reach
	@mkdir -p $(TLC_META); for r in $(3); do \
		cfg=$(TLC_META)/$(1)-$$r.cfg; \
		{ cat tla/$(2); printf 'INVARIANT %s\n' $$r; } > $$cfg; \
		if (cd tla && $(TLC) $(TLC_FLAGS) -metadir $(TLC_META)/$(1)-$$r -config $$cfg $(1).tla) 2>&1 \
			| grep -q "Invariant $$r is violated"; then \
			echo "reached:     $$r"; \
		else \
			echo "NOT REACHED: $$r"; fail=1; \
		fi; \
	done; test -z "$$fail"
endef

reach:
	$(call run_reach,ADM,ADM_reach.cfg,$(REACH))
	$(call run_reach,ADMRegions,ADMRegions_reach.cfg,$(REACH_REGIONS))

# --- Implementation model (ADMImpl.tla) ---------------------------------------

IMPL_FIXES := FixValidateFail FixStaleMeta FixNullRW FixShareType \
	FixDestroyGuard FixHartRestart FixNonRetSuspend FixStopping
REACH_IMPL := Reach_ResumedAfterRWShare Reach_RunsWhileHartStopped \
	Reach_ValidationFails Reach_RestartWhileShared Reach_StopSwitchPending
IMPL_ENV := TLC="$(TLC)" TLC_FLAGS="$(TLC_FLAGS)"

.PHONY: impl impl-original impl-ablation impl-restart check-impl

# The code as it is: report which properties hold (a report, not a test;
# the counterexamples are in tla/.tlc/impl/<property>.log).
impl:
	$(IMPL_ENV) sh tla/run-props.sh $(CURDIR)/tla/ADMImpl.cfg $(TLC_META)/impl

# The same report for the code before any fix.
impl-original:
	$(IMPL_ENV) sh tla/run-props.sh $(CURDIR)/tla/ADMImplOriginal.cfg \
		$(TLC_META)/impl-original

# Attribution: starting from the fully fixed model, turn one Fix* off at a
# time and report the properties that break.
impl-ablation:
	@for f in $(IMPL_FIXES); do \
		d=$(TLC_META)/ablation/$$f; mkdir -p $$d; \
		sed "s/$$f = TRUE/$$f = FALSE/" tla/ADMImplFixed.cfg > $$d/base.cfg; \
		echo "=== without $$f"; \
		$(IMPL_ENV) sh tla/run-props.sh $$d/base.cfg $$d | grep -v '^holds' || true; \
	done

# Ways to fix #7, compared on the fully fixed model:
#   init        restore in sm_init(false) only (no re-send)
#   started     restore in an OpenSBI hook after STARTED (needs a patch),
#               non-retentive suspend allowed
# (init_resend with non-retentive suspend allowed is the ablation of
# FixNonRetSuspend above.)
impl-restart:
	@for v in init:TRUE started:FALSE; do \
		at=$${v%%:*}; nr=$${v##*:}; \
		d=$(TLC_META)/restart/$$at; mkdir -p $$d; \
		sed -e "s/RestoreAt = \"init_resend\"/RestoreAt = \"$$at\"/" \
		    -e "s/FixNonRetSuspend = TRUE/FixNonRetSuspend = $$nr/" \
		    tla/ADMImplFixed.cfg > $$d/base.cfg; \
		echo "=== RestoreAt = $$at, FixNonRetSuspend = $$nr"; \
		$(IMPL_ENV) sh tla/run-props.sh $$d/base.cfg $$d | grep -v '^holds' || true; \
	done

# The fully fixed model: every property must hold, and the witnesses must
# be reached.
check-impl:
	cd tla && $(TLC) $(TLC_FLAGS) -metadir $(TLC_META)/check-impl3 \
		-config ADMImplFixed3.cfg ADMImpl.tla
	cd tla && $(TLC) $(TLC_FLAGS) -metadir $(TLC_META)/check-impl2e \
		-config ADMImplFixed2E.cfg ADMImpl.tla
	$(call run_reach,ADMImpl,ADMImplFixed.cfg,$(REACH_IMPL))

# --- CBMC: validate_adm_regions -------------------------------------------------

CBMC ?= cbmc
CBMC_OUT := $(CURDIR)/cbmc/.out
CBMC_FLAGS := --function harness -I cbmc/stubs -I ../sm/src -D__riscv_xlen=64 \
	--unwind 17 --unwinding-assertions
CBMC_SAFETY := --pointer-check --bounds-check --pointer-overflow-check \
	--unsigned-overflow-check --signed-overflow-check
# bound on the number of regions in B6
N_MAX ?= 2
# loops of validate_adm_regions bounded by the number of regions (cbmc
# --show-loops); a stale list makes an unwinding assertion fail
CBMC_N_LOOPS := 1 3 4 5
CBMC_UNWIND_N = $(subst $(eval) ,$(comma),$(foreach l,$(CBMC_N_LOOPS),validate_adm_regions.$(l):$(shell echo $$(($(N_MAX) + 1)))))
comma := ,

# number of allocations and frees in the PMP harness
K ?= 6
# OpenSBI headers, set by the verif devshell
OPENSBI_INCLUDE ?= $(error OPENSBI_INCLUDE is not set; enter the verif devshell)
CBMC_PMP_FLAGS = --function harness -I ../sm/src -I $(OPENSBI_INCLUDE) -D__riscv_xlen=64 \
	'-DTARGET_PLATFORM_HEADER="platform/generic/platform.h"' -DK=$(K) \
	--unwind 60 --unwinding-assertions

# A report: the failed properties of each harness (logs in cbmc/.out).
#   b7   B7 for any S > 0: memory safety, overflow, termination
#   b6   B6 (W1-W9) for at most N_MAX regions, and a witness that a valid
#        ADM with N_MAX regions exists
#   pmp  C2, C3, C5 for the region allocator after K operations, and
#        witnesses that TOR and NAPOT regions are covered
cbmc:
	@mkdir -p $(CBMC_OUT)
	@echo "=== B7"; $(CBMC) $(CBMC_FLAGS) $(CBMC_SAFETY) \
		cbmc/b7.c ../sm/src/adm.c > $(CBMC_OUT)/b7.log 2>&1; \
		grep -E ': FAILURE$$|^\*\* |^VERIFICATION' $(CBMC_OUT)/b7.log
	@echo "=== B6 (N_MAX = $(N_MAX))"; $(CBMC) $(CBMC_FLAGS) -DN_MAX=$(N_MAX) \
		--unwindset $(CBMC_UNWIND_N) \
		cbmc/b6.c ../sm/src/adm.c > $(CBMC_OUT)/b6.log 2>&1; \
		grep -E ': FAILURE$$|^\*\* |^VERIFICATION' $(CBMC_OUT)/b6.log
	@$(CBMC) $(CBMC_FLAGS) -DN_MAX=$(N_MAX) -DREACH --unwindset $(CBMC_UNWIND_N) \
		cbmc/b6.c ../sm/src/adm.c > $(CBMC_OUT)/b6-reach.log 2>&1; \
		if grep -q 'reach: .*: FAILURE' $(CBMC_OUT)/b6-reach.log; \
		then echo "reached:     valid ADM with $(N_MAX) regions"; \
		else echo "NOT REACHED: valid ADM with $(N_MAX) regions"; fi
	@echo "=== PMP (K = $(K))"; $(CBMC) $(CBMC_PMP_FLAGS) cbmc/pmp_regions.c \
		> $(CBMC_OUT)/pmp.log 2>&1; \
		grep -E ': FAILURE$$|^\*\* |^VERIFICATION' $(CBMC_OUT)/pmp.log
	@$(CBMC) $(CBMC_PMP_FLAGS) -DREACH cbmc/pmp_regions.c > $(CBMC_OUT)/pmp-reach.log 2>&1; \
		for r in TOR NAPOT; do \
			if grep -q "reach: a $$r region.*: FAILURE" $(CBMC_OUT)/pmp-reach.log; \
			then echo "reached:     $$r region"; else echo "NOT REACHED: $$r region"; fi; \
		done

# --- Rocq proofs ---------------------------------------------------------------

ROCQ ?= rocq
ROCQ_FLAGS := -Q rocq ADMVerif
ROCQ_LOG := rocq/.assumptions.log

rocq/%.vo: rocq/%.v
	$(ROCQ) compile $(ROCQ_FLAGS) $<

rocq/ADMRegions.vo: rocq/ADM.vo

# Design model: every property is proved for all reachable states, and no
# theorem depends on an axiom (each Print Assumptions reports "Closed").
#   ADM.v         groups A, B, D, E   (4-5 min, 3.5 GB peak)
#   ADMRegions.v  group C
prove: rocq/ADM.vo rocq/ADMRegions.vo
	$(ROCQ) compile $(ROCQ_FLAGS) rocq/Assumptions.v > $(ROCQ_LOG)
	@cat $(ROCQ_LOG)
	@n=$$(grep -c '^Closed under the global context' $(ROCQ_LOG)); \
	 t=$$(grep -c '^Print Assumptions' rocq/Assumptions.v); \
	 test "$$n" = "$$t" || { echo "some theorem depends on an axiom"; exit 1; }

clean:
	rm -f $(PDFS)
	rm -rf tla/.tlc cbmc/.out
	rm -f rocq/*.vo rocq/*.vok rocq/*.vos rocq/*.glob rocq/.*.aux $(ROCQ_LOG)
