#include "World/OGWorldTimeService.h"

#include "Algo/Reverse.h"

namespace
{
bool CheckedSubtractInt64(
    int64 A,
    int64 B,
    int64& Out)
{
    if ((B > 0 && A < MIN_int64 + B) ||
        (B < 0 && A > MAX_int64 + B))
    {
        return false;
    }

    Out = A - B;
    return true;
}

bool CheckedMultiplyByPositiveInt64(
    int64 Value,
    int64 PositiveMultiplier,
    int64& Out)
{
    if (PositiveMultiplier <= 0)
    {
        return false;
    }

    if ((Value > 0 &&
         Value > MAX_int64 / PositiveMultiplier) ||
        (Value < 0 &&
         Value < MIN_int64 / PositiveMultiplier))
    {
        return false;
    }

    Out =
        Value * PositiveMultiplier;
    return true;
}

bool CheckedAddInt64(
    int64 A,
    int64 B,
    int64& Out)
{
    if ((B > 0 && A > MAX_int64 - B) ||
        (B < 0 && A < MIN_int64 - B))
    {
        return false;
    }

    Out = A + B;
    return true;
}
}

bool FOGWorldTimeService::SaveTimeDomain(
    const FOGTimeDomainRecord& Domain,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Domain.TimeDomainId.IsValid() ||
        Domain.RateNumerator <= 0 ||
        Domain.RateDenominator <= 0 ||
        CreatedWorldTick < 0)
    {
        OutError =
            TEXT("Time Domain save request is invalid.");
        return false;
    }

    if (Domain.ParentTimeDomainId.IsValid())
    {
        TSet<FOGEntityId> Visited;
        FOGEntityId Cursor =
            Domain.ParentTimeDomainId;

        while (Cursor.IsValid())
        {
            if (Cursor ==
                Domain.TimeDomainId)
            {
                OutError =
                    TEXT("Time Domain hierarchy would create a cycle.");
                return false;
            }

            if (Visited.Contains(Cursor))
            {
                OutError =
                    TEXT("Existing Time Domain hierarchy already contains a cycle.");
                return false;
            }

            Visited.Add(Cursor);

            bool bFound = false;
            FOGTimeDomainRecord Parent;
            if (!Store.TryReadTimeDomain(
                    Cursor,
                    bFound,
                    Parent,
                    OutError))
            {
                return false;
            }

            if (!bFound)
            {
                OutError =
                    TEXT("Time Domain parent does not exist.");
                return false;
            }

            Cursor =
                Parent.ParentTimeDomainId;
        }
    }

    return Store.UpsertTimeDomain(
        Domain,
        CreatedWorldTick,
        OutError);
}

bool FOGWorldTimeService::BuildDomainChain(
    const FOGEntityId& TimeDomainId,
    TArray<FOGTimeDomainRecord>& OutRootToLeaf,
    FString& OutError) const
{
    OutRootToLeaf.Reset();
    OutError.Reset();

    if (!TimeDomainId.IsValid())
    {
        OutError =
            TEXT("Time projection requires a valid Time Domain ID.");
        return false;
    }

    TSet<FOGEntityId> Visited;
    FOGEntityId Cursor =
        TimeDomainId;

    while (Cursor.IsValid())
    {
        if (Visited.Contains(Cursor))
        {
            OutError =
                TEXT("Time Domain hierarchy contains a cycle.");
            OutRootToLeaf.Reset();
            return false;
        }

        Visited.Add(Cursor);

        bool bFound = false;
        FOGTimeDomainRecord Domain;
        if (!Store.TryReadTimeDomain(
                Cursor,
                bFound,
                Domain,
                OutError))
        {
            OutRootToLeaf.Reset();
            return false;
        }

        if (!bFound)
        {
            OutError =
                TEXT("Time projection references an unknown Time Domain.");
            OutRootToLeaf.Reset();
            return false;
        }

        OutRootToLeaf.Add(
            Domain);
        Cursor =
            Domain.ParentTimeDomainId;
    }

    Algo::Reverse(
        OutRootToLeaf);
    return true;
}

bool FOGWorldTimeService::ProjectOneDomain(
    const FOGTimeDomainRecord& Domain,
    int64 ParentTick,
    int64& OutLocalTick,
    FString& OutError)
{
    if (Domain.RateNumerator <= 0 ||
        Domain.RateDenominator <= 0)
    {
        OutError =
            TEXT("Time Domain has an invalid rational rate.");
        return false;
    }

    int64 Delta = 0;
    if (!CheckedSubtractInt64(
            ParentTick,
            Domain.ParentEpochTick,
            Delta))
    {
        OutError =
            TEXT("Time projection overflowed while computing epoch delta.");
        return false;
    }

    const int64 Whole =
        Delta /
        Domain.RateDenominator;
    const int64 Remainder =
        Delta %
        Domain.RateDenominator;

    int64 WholeScaled = 0;
    int64 RemainderScaled = 0;

    if (!CheckedMultiplyByPositiveInt64(
            Whole,
            Domain.RateNumerator,
            WholeScaled) ||
        !CheckedMultiplyByPositiveInt64(
            Remainder,
            Domain.RateNumerator,
            RemainderScaled))
    {
        OutError =
            TEXT("Time projection overflowed while applying rational rate.");
        return false;
    }

    const int64 FractionScaled =
        RemainderScaled /
        Domain.RateDenominator;

    int64 ScaledDelta = 0;
    if (!CheckedAddInt64(
            WholeScaled,
            FractionScaled,
            ScaledDelta) ||
        !CheckedAddInt64(
            Domain.LocalEpochTick,
            ScaledDelta,
            OutLocalTick))
    {
        OutError =
            TEXT("Time projection overflowed while applying local epoch.");
        return false;
    }

    return true;
}

bool FOGWorldTimeService::ProjectCanonicalTick(
    const FOGEntityId& TimeDomainId,
    int64 CanonicalWorldTick,
    FOGLocalTimeProjection& OutProjection,
    FString& OutError) const
{
    OutProjection =
        FOGLocalTimeProjection();
    OutError.Reset();

    if (CanonicalWorldTick < 0)
    {
        OutError =
            TEXT("Canonical world tick cannot be negative.");
        return false;
    }

    TArray<FOGTimeDomainRecord> Chain;
    if (!BuildDomainChain(
            TimeDomainId,
            Chain,
            OutError))
    {
        return false;
    }

    int64 CurrentTick =
        CanonicalWorldTick;

    for (const FOGTimeDomainRecord& Domain :
         Chain)
    {
        int64 NextTick = 0;
        if (!ProjectOneDomain(
                Domain,
                CurrentTick,
                NextTick,
                OutError))
        {
            return false;
        }

        CurrentTick =
            NextTick;
    }

    OutProjection.TimeDomainId =
        TimeDomainId;
    OutProjection.CanonicalWorldTick =
        CanonicalWorldTick;
    OutProjection.LocalTick =
        CurrentTick;

    if (!Chain.IsEmpty())
    {
        OutProjection.CalendarId =
            Chain.Last().CalendarId;
    }

    return true;
}

bool FOGWorldTimeService::HasStrictlyMoreThanCalendarDuration(
    const FOGEntityId& TimeDomainId,
    int64 StartCanonicalWorldTick,
    int64 EndCanonicalWorldTick,
    FName DurationKind,
    int32 DurationCount,
    const FOGCalendarElapsedResolver& Resolver,
    bool& bOutElapsed,
    FString& OutError) const
{
    bOutElapsed = false;
    OutError.Reset();

    if (StartCanonicalWorldTick < 0 ||
        EndCanonicalWorldTick <
            StartCanonicalWorldTick ||
        DurationKind.IsNone() ||
        DurationCount <= 0 ||
        !Resolver)
    {
        OutError =
            TEXT("Calendar-duration query is invalid.");
        return false;
    }

    FOGLocalTimeProjection Start;
    FOGLocalTimeProjection End;

    if (!ProjectCanonicalTick(
            TimeDomainId,
            StartCanonicalWorldTick,
            Start,
            OutError) ||
        !ProjectCanonicalTick(
            TimeDomainId,
            EndCanonicalWorldTick,
            End,
            OutError))
    {
        return false;
    }

    if (!Start.CalendarId.IsValid() ||
        Start.CalendarId !=
            End.CalendarId)
    {
        OutError =
            TEXT("Calendar-duration query requires one stable authored calendar.");
        return false;
    }

    FOGCalendarElapsedQuery Query;
    Query.CalendarId =
        Start.CalendarId;
    Query.StartLocalTick =
        Start.LocalTick;
    Query.EndLocalTick =
        End.LocalTick;
    Query.DurationKind =
        DurationKind;
    Query.DurationCount =
        DurationCount;
    Query.bStrictlyMoreThan =
        true;

    return Resolver(
        Query,
        bOutElapsed,
        OutError);
}
