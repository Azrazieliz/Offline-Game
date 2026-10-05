#include "Progression/OGRankService.h"

bool FOGRankService::SetRankState(
    const FOGEntityRankStateRecord& State,
    FString& OutError)
{
    // Ordering and historical-peak correctness require content Rank metadata;
    // callers provide the already-resolved attained/effective/peak identities.
    return Store.UpsertEntityRankState(
        State,
        OutError);
}

bool FOGRankService::ResolveEffectiveRank(
    const FOGEntityId& EntityId,
    bool& bOutFound,
    FOGResolvedRankProjection& OutProjection,
    FString& OutError) const
{
    bOutFound = false;
    OutProjection =
        FOGResolvedRankProjection();

    FOGEntityRankStateRecord State;
    if (!Store.TryReadEntityRankState(
            EntityId,
            bOutFound,
            State,
            OutError))
    {
        return false;
    }

    if (!bOutFound)
    {
        return true;
    }

    if (State.EffectiveRankId.IsValid())
    {
        OutProjection.RankId =
            State.EffectiveRankId;
        OutProjection.Level =
            State.EffectiveLevel;
        OutProjection.bUsingEffectiveOverride =
            true;
    }
    else
    {
        OutProjection.RankId =
            State.AttainedRankId;
        OutProjection.Level =
            State.AttainedLevel;
        OutProjection.bUsingEffectiveOverride =
            false;
    }

    return true;
}
