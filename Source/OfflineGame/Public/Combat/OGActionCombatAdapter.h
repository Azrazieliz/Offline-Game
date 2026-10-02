#pragma once

#include "CoreMinimal.h"
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
};
