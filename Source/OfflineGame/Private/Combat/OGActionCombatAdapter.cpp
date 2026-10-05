#include "Combat/OGActionCombatAdapter.h"

bool FOGActionCombatAdapter::ValidateSwitchParty(
    const TArray<FOGCombatUnitState>& Party,
    FString& OutError)
{
    return ValidateSwitchParty(
        Party,
        FOGIdentityExclusivityContext(),
        OutError);
}

bool FOGActionCombatAdapter::ValidateSwitchParty(
    const TArray<FOGCombatUnitState>& Party,
    const FOGIdentityExclusivityContext& IdentityContext,
    FString& OutError)
{
    OutError.Reset();

    // Ruler + up to two switch/QTE companions.
    if (Party.IsEmpty() ||
        Party.Num() > 3)
    {
        OutError =
            TEXT("World Mode action party must contain one to three characters.");
        return false;
    }

    TSet<FOGEntityId> Units;

    for (const FOGCombatUnitState& Unit :
         Party)
    {
        if (!Unit.UnitEntityId.IsValid() ||
            !Unit.IdentityId.IsValid())
        {
            OutError =
                TEXT("Action party contains invalid unit/identity data.");
            return false;
        }

        if (Units.Contains(
                Unit.UnitEntityId))
        {
            OutError =
                TEXT("Action party contains the same Manifestation more than once.");
            return false;
        }

        Units.Add(
            Unit.UnitEntityId);
    }

    return ValidateLocalIdentityExclusivity(
        Party,
        IdentityContext,
        OutError);
}

bool FOGActionCombatAdapter::ResolveRankSuppressionMultiplier(
    const FOGCombatUnitState& Source,
    const FOGCombatUnitState& Target,
    FName ChannelId,
    const FOGRankSuppressionResolver& Resolver,
    int32& OutMultiplierBps,
    FString& OutError)
{
    return FOGCombatRankHooks::ResolveChannelMultiplier(
        Source,
        Target,
        ChannelId,
        Resolver,
        OutMultiplierBps,
        OutError);
}
