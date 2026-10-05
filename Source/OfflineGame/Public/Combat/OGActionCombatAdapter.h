#pragma once

#include "CoreMinimal.h"
#include "Combat/OGCombatIdentityRules.h"
#include "Combat/OGCombatRankHooks.h"
#include "Combat/OGCombatTypes.h"

/**
 * Shared state bridge for World Mode action combat.
 *
 * The action layer may own animation, movement, hit traces and timing, but the
 * character kit/state originates from the same combat snapshot used by turn mode.
 */
class OFFLINEGAME_API FOGActionCombatAdapter
{
public:
    static FOGCombatUnitState MakeActionSnapshot(
        const FOGCombatUnitState& AuthoritativeUnit)
    {
        return AuthoritativeUnit;
    }

    static bool ValidateSwitchParty(
        const TArray<FOGCombatUnitState>& Party,
        FString& OutError);

    static bool ValidateSwitchParty(
        const TArray<FOGCombatUnitState>& Party,
        const FOGIdentityExclusivityContext& IdentityContext,
        FString& OutError);

    static bool ResolveRankSuppressionMultiplier(
        const FOGCombatUnitState& Source,
        const FOGCombatUnitState& Target,
        FName ChannelId,
        const FOGRankSuppressionResolver& Resolver,
        int32& OutMultiplierBps,
        FString& OutError);
};
