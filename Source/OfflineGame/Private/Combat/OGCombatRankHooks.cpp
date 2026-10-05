#include "Combat/OGCombatRankHooks.h"

namespace OGRankSuppressionChannels
{
FName Damage()
{
    static const FName Name(
        TEXT("damage"));
    return Name;
}

FName EffectPenetration()
{
    static const FName Name(
        TEXT("effect_penetration"));
    return Name;
}

FName ResistanceBreak()
{
    static const FName Name(
        TEXT("resistance_break"));
    return Name;
}

FName Control()
{
    static const FName Name(
        TEXT("control"));
    return Name;
}

FName Perception()
{
    static const FName Name(
        TEXT("perception"));
    return Name;
}

FName PresenceTolerance()
{
    static const FName Name(
        TEXT("presence_tolerance"));
    return Name;
}
}

bool FOGCombatRankHooks::ResolveChannelMultiplier(
    const FOGCombatUnitState& Source,
    const FOGCombatUnitState& Target,
    FName ChannelId,
    const FOGRankSuppressionResolver& Resolver,
    int32& OutMultiplierBps,
    FString& OutError)
{
    OutMultiplierBps = 10000;
    OutError.Reset();

    if (!Source.UnitEntityId.IsValid() ||
        !Target.UnitEntityId.IsValid() ||
        ChannelId.IsNone())
    {
        OutError =
            TEXT("Rank-suppression query contains invalid combat context.");
        return false;
    }

    const bool bSourceRankKnown =
        Source.RankProjection.RankId.IsValid();
    const bool bTargetRankKnown =
        Target.RankProjection.RankId.IsValid();

    if (!bSourceRankKnown ||
        !bTargetRankKnown)
    {
        // Unknown Rank is a knowledge/projection absence, not permission to
        // invent a gap. Ordinary combat math proceeds without a Rank modifier.
        return true;
    }

    if (!Resolver)
    {
        OutError =
            TEXT("Resolved Rank interaction requires a data/tuning Rank-suppression resolver.");
        return false;
    }

    FOGRankSuppressionQuery Query;
    Query.SourceUnitId =
        Source.UnitEntityId;
    Query.TargetUnitId =
        Target.UnitEntityId;
    Query.SourceRankId =
        Source.RankProjection.RankId;
    Query.TargetRankId =
        Target.RankProjection.RankId;
    Query.SourceLevel =
        Source.RankProjection.Level;
    Query.TargetLevel =
        Target.RankProjection.Level;
    Query.ChannelId =
        ChannelId;

    if (!Resolver(
            Query,
            OutMultiplierBps,
            OutError))
    {
        return false;
    }

    if (OutMultiplierBps < 0 ||
        OutMultiplierBps > 10000)
    {
        OutError =
            TEXT("Rank-suppression resolver returned an invalid basis-point multiplier.");
        OutMultiplierBps = 10000;
        return false;
    }

    return true;
}
