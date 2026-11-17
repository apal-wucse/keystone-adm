(** Every theorem of ADM.v must be closed under the global context, i.e.
    depend on no axiom.  [make prove] checks the output of this file. *)

From ADMVerif Require Import ADM ADMRegions.

Print Assumptions inv_reachable.
Print Assumptions A1_InitialProtection.
Print Assumptions A2_ExclusiveWhileRunning.
Print Assumptions A3_OnlyExplicitGrant.
Print Assumptions A4_RevokedBeforeEnter.
Print Assumptions A6_IsolatedWhenNotShared.
Print Assumptions A7_ShareStateIntegrity.
Print Assumptions A8_InvoluntaryStopSafe.
Print Assumptions A9_NoSecretAtRelease.
Print Assumptions E1_NoUnintendedDisclosure.
Print Assumptions B1_ValidatedBeforeRun.
Print Assumptions B2_NoAccessDuringValidation.
Print Assumptions B3_FailClosed.
Print Assumptions B4_FreshMetadata.
Print Assumptions B5_FreshNeedsValidation.
Print Assumptions B5_EnterOnlyViaValidation.
Print Assumptions E2_ContentsPreserved.
Print Assumptions D1_CallerRestriction.
Print Assumptions D2a_RunningConsistent.
Print Assumptions D2b_MetaMatchesSent.
Print Assumptions D2c_InvalidConsistent.
Print Assumptions D2d_SingleRunner.

Print Assumptions step_cases.
Print Assumptions reachableR_inv.
Print Assumptions C1_RegionsDisjoint_holds.
Print Assumptions C_RegionsFollowLifecycle_holds.
Print Assumptions C2_SMNotShadowed.
Print Assumptions C_EPMIsolated.
Print Assumptions C_ADMPermFaithful.
