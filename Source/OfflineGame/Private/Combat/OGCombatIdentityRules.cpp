#include "Combat/OGCombatIdentityRules.h"

bool ValidateLocalIdentityExclusivity(
    const TArray<FOGCombatUnitState>& Units,
    const FOGIdentityExclusivityContext& Context,
    FString& OutError)
{
    OutError.Reset();

    TMap<FOGContentId, const FOGCombatUnitState*> FirstByIdentity;

    for (const FOGCombatUnitState& Unit : Units)
    {
        if (!Unit.UnitEntityId.IsValid() ||
            !Unit.IdentityId.IsValid())
        {
            OutError =
                TEXT("Identity-exclusivity validation received invalid unit/identity data.");
            return false;
        }

        const FOGCombatUnitState* const* Existing =
            FirstByIdentity.Find(Unit.IdentityId);

        if (!Existing)
        {
            FirstByIdentity.Add(
                Unit.IdentityId,
                &Unit);
            continue;
        }

        EOGIdentityExclusivityOverrideDecision Decision =
            EOGIdentityExclusivityOverrideDecision::UseDefault;

        if (Context.Override)
        {
            Decision =
                Context.Override(
                    **Existing,
                    Unit);
        }

        if (Decision ==
            EOGIdentityExclusivityOverrideDecision::AllowDuplicate)
        {
            continue;
        }

        OutError = FString::Printf(
            TEXT("Local encounter violates Character Identity exclusivity for '%s'."),
            *Unit.IdentityId.ToString());
        return false;
    }

    return true;
}
