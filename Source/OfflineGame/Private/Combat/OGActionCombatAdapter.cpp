#include "Combat/OGActionCombatAdapter.h"

bool FOGActionCombatAdapter::ValidateSwitchParty(
    const TArray<FOGCombatUnitState>& Party,
    FString& OutError)
{
    OutError.Reset();

    // Ruler + up to two switch/QTE companions.
    if (Party.IsEmpty() || Party.Num() > 3)
    {
        OutError = TEXT("World Mode action party must contain one to three characters.");
        return false;
    }

    TSet<FOGEntityId> Units;
    TSet<FOGContentId> Identities;

    for (const FOGCombatUnitState& Unit : Party)
    {
        if (!Unit.UnitEntityId.IsValid() || !Unit.IdentityId.IsValid())
        {
            OutError = TEXT("Action party contains invalid unit/identity data.");
            return false;
        }

        if (Units.Contains(Unit.UnitEntityId))
        {
            OutError = TEXT("Action party contains the same Manifestation more than once.");
            return false;
        }

        if (Identities.Contains(Unit.IdentityId))
        {
            OutError = TEXT("Action party violates local Character Identity exclusivity.");
            return false;
        }

        Units.Add(Unit.UnitEntityId);
        Identities.Add(Unit.IdentityId);
    }

    return true;
}
