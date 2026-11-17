DOCS := MODEL MODEL.en
PDFS := $(addsuffix .pdf,$(DOCS))

ASCIIDOCTOR_PDF ?= asciidoctor-pdf
# Directory containing ipaexm.ttf; set by the verif devshell
VERIF_FONTS_DIR ?= $(error VERIF_FONTS_DIR is not set; enter the verif devshell)

ADOC_FLAGS := --failure-level WARN \
	-a pdf-themesdir=docs/theme \
	-a pdf-theme=model \
	-a pdf-fontsdir="$(VERIF_FONTS_DIR);GEM_FONTS_DIR"

.PHONY: all docs check reach prove clean
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
	rm -rf tla/.tlc
	rm -f rocq/*.vo rocq/*.vok rocq/*.vos rocq/*.glob rocq/.*.aux $(ROCQ_LOG)
