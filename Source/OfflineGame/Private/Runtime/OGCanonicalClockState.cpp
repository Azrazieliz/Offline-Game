#include "Runtime/OGCanonicalClockState.h"

bool FOGCanonicalClockAccumulator::Advance(
    const FOGCanonicalClockState& Committed,
    int64 ElapsedSourceQuanta,
    int64 ResolvedRateNumerator,
    FOGCanonicalClockState& OutCandidate,
    FString& OutError)
{
    OutCandidate = Committed;
    OutError.Reset();
    if (Committed.CurrentWorldTick < 0 ||
        Committed.RateDenominator <= 0 ||
        Committed.FractionalNumerator < 0 ||
        Committed.FractionalNumerator >= Committed.RateDenominator ||
        ElapsedSourceQuanta < 0 || ResolvedRateNumerator < 0)
    {
        OutError = TEXT("Canonical clock state or resolved elapsed input is invalid.");
        return false;
    }

    // A zero rate is an explicit core pause, not a persisted TimeDomain rate.
    if (ElapsedSourceQuanta == 0 || ResolvedRateNumerator == 0)
    {
        return true;
    }

    // Fast normal path, plus an exact 63-step division path for very long
    // absences. An intermediate product overflow must not impose a time cap
    // when the final quotient is still representable.
    int64 Whole = 0;
    int64 Remainder = 0;
    if (ElapsedSourceQuanta <= MAX_int64 / ResolvedRateNumerator)
    {
        const int64 Product = ElapsedSourceQuanta * ResolvedRateNumerator;
        Whole = Product / Committed.RateDenominator;
        Remainder = Product % Committed.RateDenominator;
    }
    else
    {
        const int64 AddWhole = ResolvedRateNumerator / Committed.RateDenominator;
        const int64 AddResidue = ResolvedRateNumerator % Committed.RateDenominator;
        for (int32 Bit = 62; Bit >= 0; --Bit)
        {
            if (Whole > MAX_int64 / 2)
            { OutError = TEXT("Canonical clock advance exceeds monotonic tick range."); return false; }
            Whole *= 2;
            const int64 DoubleRoom = Committed.RateDenominator - Remainder;
            const bool bDoubleCarry = Remainder >= DoubleRoom;
            Remainder = bDoubleCarry ? Remainder - DoubleRoom : Remainder + Remainder;
            if (bDoubleCarry)
            {
                if (Whole == MAX_int64)
                { OutError = TEXT("Canonical clock advance exceeds monotonic tick range."); return false; }
                ++Whole;
            }
            if ((static_cast<uint64>(ElapsedSourceQuanta) & (1ull << Bit)) != 0)
            {
                if (AddWhole > MAX_int64 - Whole)
                { OutError = TEXT("Canonical clock advance exceeds monotonic tick range."); return false; }
                Whole += AddWhole;
                const int64 AddRoom = Committed.RateDenominator - Remainder;
                const bool bAddCarry = AddResidue >= AddRoom;
                Remainder = bAddCarry ? AddResidue - AddRoom : Remainder + AddResidue;
                if (bAddCarry)
                {
                    if (Whole == MAX_int64)
                    { OutError = TEXT("Canonical clock advance exceeds monotonic tick range."); return false; }
                    ++Whole;
                }
            }
        }
    }

    // Add residues without overflowing when the denominator is near MAX_int64.
    const int64 ResidueRoom = Committed.RateDenominator - Committed.FractionalNumerator;
    const bool bCarry = Remainder >= ResidueRoom;
    const int64 NewResidue = bCarry
        ? Remainder - ResidueRoom
        : Committed.FractionalNumerator + Remainder;
    if (Whole > MAX_int64 - Committed.CurrentWorldTick ||
        (bCarry && Whole == MAX_int64 - Committed.CurrentWorldTick))
    {
        OutError = TEXT("Canonical clock advance exceeds monotonic tick range.");
        return false;
    }

    OutCandidate.CurrentWorldTick = Committed.CurrentWorldTick + Whole + (bCarry ? 1 : 0);
    OutCandidate.FractionalNumerator = NewResidue;
    return true;
}
