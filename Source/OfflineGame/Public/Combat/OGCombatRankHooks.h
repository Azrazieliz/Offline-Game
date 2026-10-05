#pragma once

#include "CoreMinimal.h"
#include "Combat/OGCombatTypes.h"

/**
 * Query passed to data/tuning-owned Rank suppression resolution.
 *
 * Rank names/ordering and suppression envelopes are not hard-coded here.
 */
struct FOGRankSuppressionQuery
{
    FOGEntityId SourceUnitId;
    FOGEntityId TargetUnitId;
    FOGContentId SourceRankId;
    FOGContentId TargetRankId;
    int32 SourceLevel = 1;
    int32 TargetLevel = 1;
    FName ChannelId = NAME_None;
};

/**
 * Returns a basis-point multiplier in [0,10000] for the supplied interaction.
 * The implementation owns Rank ordering, gap interpretation, tuning profile and
 * explicit bypass mechanics.
 */
using FOGRankSuppressionResolver =
    TFunction<bool(
        const FOGRankSuppressionQuery& Query,
        int32& OutMultiplierBps,
        FString& OutError)>;

namespace OGRankSuppressionChannels
{
    OFFLINEGAME_API FName Damage();
    OFFLINEGAME_API FName EffectPenetration();
    OFFLINEGAME_API FName ResistanceBreak();
    OFFLINEGAME_API FName Control();
    OFFLINEGAME_API FName Perception();
    OFFLINEGAME_API FName PresenceTolerance();
}

/**
 * Shared Rank hook consumed by both combat executors.
 *
 * Combat owns neither Rank ordering nor tuning coefficients. When both units
 * carry resolved Rank projections, a resolver is required.
 */
class OFFLINEGAME_API FOGCombatRankHooks
{
public:
    static bool ResolveChannelMultiplier(
        const FOGCombatUnitState& Source,
        const FOGCombatUnitState& Target,
        FName ChannelId,
        const FOGRankSuppressionResolver& Resolver,
        int32& OutMultiplierBps,
        FString& OutError);
};
