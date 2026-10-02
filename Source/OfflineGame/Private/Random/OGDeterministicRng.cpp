#include "Random/OGDeterministicRng.h"

uint64 FOGDeterministicRng::NextUInt64()
{
    // SplitMix64.
    uint64 Z = (State += 0x9E3779B97F4A7C15ull);
    Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
    Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
    Z ^= (Z >> 31);

    ++DrawCount;
    return Z;
}

uint32 FOGDeterministicRng::NextUInt32()
{
    return static_cast<uint32>(NextUInt64() >> 32);
}

int32 FOGDeterministicRng::NextRange(
    int32 MinInclusive,
    int32 MaxExclusive)
{
    if (MaxExclusive <= MinInclusive)
    {
        return MinInclusive;
    }

    const uint64 Span64 =
        static_cast<uint64>(
            static_cast<int64>(MaxExclusive) -
            static_cast<int64>(MinInclusive));

    const uint32 Span = static_cast<uint32>(Span64);
    const uint32 Threshold = (0u - Span) % Span;

    while (true)
    {
        const uint32 Value = NextUInt32();
        if (Value >= Threshold)
        {
            const int64 Result =
                static_cast<int64>(MinInclusive) +
                static_cast<int64>(Value % Span);

            return static_cast<int32>(Result);
        }
    }
}

float FOGDeterministicRng::NextUnitFloat()
{
    const uint32 Fraction = NextUInt32() >> 8;
    return static_cast<float>(Fraction) / 16777216.0f;
}
