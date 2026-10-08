#pragma once

#include "CoreMinimal.h"

/**
 * Canonical integer clock arithmetic. Source quanta have no calendar meaning.
 * The owning runtime validates/persists authored rates, converts elapsed source
 * milliseconds and dispatches world work. No seconds/day/month default exists
 * in this arithmetic layer.
 */
struct FOGCanonicalClockState
{
    int64 CurrentWorldTick = 0;
    int64 FractionalNumerator = 0;
    int64 RateDenominator = 0;
};

/** Pure candidate transition. Only the owning Core publishes/persists the result.
 * World work is retried from persisted Director schedules after clock commit.
 * Never call this arithmetic from presentation actors.
 */
class OFFLINEGAME_API FOGCanonicalClockAccumulator
{
public:
    static bool Advance(
        const FOGCanonicalClockState& Committed,
        int64 ElapsedSourceQuanta,
        int64 ResolvedRateNumerator,
        FOGCanonicalClockState& OutCandidate,
        FString& OutError);
};
