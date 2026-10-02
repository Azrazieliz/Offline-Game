#pragma once

#include "CoreMinimal.h"

/**
 * Small deterministic PRNG for reproducible gameplay decisions.
 *
 * Seed and draw count are sufficient provenance for deterministic replay.
 * This is not cryptographic and must never be used for security-sensitive work.
 */
class OFFLINEGAME_API FOGDeterministicRng
{
public:
    explicit FOGDeterministicRng(uint64 InSeed = 0)
        : State(InSeed)
    {
    }

    uint64 NextUInt64();
    uint32 NextUInt32();

    /** Inclusive lower bound, exclusive upper bound. */
    int32 NextRange(int32 MinInclusive, int32 MaxExclusive);

    /** Deterministic [0,1) value with 24 bits of fractional precision. */
    float NextUnitFloat();

    uint64 GetState() const { return State; }
    uint64 GetDrawCount() const { return DrawCount; }

private:
    uint64 State = 0;
    uint64 DrawCount = 0;
};
