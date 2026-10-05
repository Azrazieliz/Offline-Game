#pragma once

#include "CoreMinimal.h"
#include "Combat/OGCombatTypes.h"

/**
 * Explicit exception result for the default local Character-Identity rule.
 *
 * UseDefault preserves the frozen default: the same canonical Character
 * Identity may not appear more than once in one local encounter.
 */
enum class EOGIdentityExclusivityOverrideDecision : uint8
{
    UseDefault,
    AllowDuplicate,
    RejectDuplicate
};

struct FOGIdentityExclusivityContext
{
    /**
     * Optional mechanic-specific exception hook. It is evaluated only when two
     * units share one Character Identity.
     */
    TFunction<EOGIdentityExclusivityOverrideDecision(
        const FOGCombatUnitState& ExistingUnit,
        const FOGCombatUnitState& CandidateUnit)> Override;
};

/**
 * Shared local encounter validator used by both action and turn executors.
 *
 * Default: one Manifestation of a canonical Character Identity in the entire
 * local encounter. Explicit mechanics may allow a specific duplicate pair.
 */
OFFLINEGAME_API bool ValidateLocalIdentityExclusivity(
    const TArray<FOGCombatUnitState>& Units,
    const FOGIdentityExclusivityContext& Context,
    FString& OutError);
