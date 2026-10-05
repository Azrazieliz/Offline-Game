#include "World/OGSovereigntyService.h"

bool FOGSovereigntyService::IsEffectiveState(
    FName ControlState)
{
    return ControlState == FName(TEXT("effective")) ||
        ControlState == FName(TEXT("controlled"));
}

bool FOGSovereigntyService::RefreshSovereigntyState(
    const FOGEntityId& RulerId,
    int64 WorldTick,
    bool bMeetsOverlordRequirements,
    const FString& ScopeStateJson,
    FOGRulerSovereigntyStateRecord& OutState,
    FString& OutError)
{
    OutState = FOGRulerSovereigntyStateRecord();
    OutError.Reset();

    if (!RulerId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Sovereignty refresh requires a valid Ruler and non-negative world tick.");
        return false;
    }

    TArray<FOGTerritoryClaimRecord> Claims;
    if (!Store.ListTerritoryClaimsByRuler(
            RulerId,
            Claims,
            OutError))
    {
        return false;
    }

    bool bHasEffectiveControl = false;
    bool bHasGraceContinuity = false;
    bool bHaveContinuityStart = false;
    int64 EarliestContinuityStart = 0;

    for (const FOGTerritoryClaimRecord& Claim :
         Claims)
    {
        const bool bEffective =
            IsEffectiveState(
                Claim.ControlState);
        const bool bInGrace =
            Claim.ControlState ==
                FName(TEXT("displaced")) &&
            Claim.bHasReclaimDeadline &&
            WorldTick <=
                Claim.ReclaimDeadlineWorldTick;

        if (!bEffective &&
            !bInGrace)
        {
            continue;
        }

        bHasEffectiveControl |=
            bEffective;
        bHasGraceContinuity |=
            bInGrace;

        if (Claim.bHasEffectiveControlStart)
        {
            if (!bHaveContinuityStart ||
                Claim.EffectiveControlStartWorldTick <
                    EarliestContinuityStart)
            {
                bHaveContinuityStart = true;
                EarliestContinuityStart =
                    Claim.EffectiveControlStartWorldTick;
            }
        }
        else if (!bHaveContinuityStart ||
                 Claim.ClaimStartWorldTick <
                    EarliestContinuityStart)
        {
            bHaveContinuityStart = true;
            EarliestContinuityStart =
                Claim.ClaimStartWorldTick;
        }
    }

    bool bExistingFound = false;
    FOGRulerSovereigntyStateRecord Existing;
    if (!Store.TryReadRulerSovereigntyState(
            RulerId,
            bExistingFound,
            Existing,
            OutError))
    {
        return false;
    }

    FOGRulerSovereigntyStateRecord State;
    State.RulerId = RulerId;
    State.ScopeStateJson =
        ScopeStateJson.IsEmpty()
            ? TEXT("{}")
            : ScopeStateJson;
    State.UpdatedWorldTick =
        WorldTick;

    const bool bHasSovereigntyContinuity =
        bHasEffectiveControl ||
        bHasGraceContinuity;

    if (bHasSovereigntyContinuity)
    {
        State.CurrentTitle =
            bMeetsOverlordRequirements
                ? FName(TEXT("overlord"))
                : FName(TEXT("ruler"));
        State.bHasContinuousControlStart =
            bHaveContinuityStart;
        State.ContinuousControlStartWorldTick =
            bHaveContinuityStart
                ? EarliestContinuityStart
                : WorldTick;
    }
    else
    {
        // Persistence sentinel for "no active universal sovereignty title".
        State.CurrentTitle =
            FName(TEXT("none"));
    }

    if (bExistingFound)
    {
        State.HistoricalPeakTitle =
            Existing.HistoricalPeakTitle;

        if (Existing.bHasLastEffectiveControlTick)
        {
            State.bHasLastEffectiveControlTick = true;
            State.LastEffectiveControlWorldTick =
                Existing.LastEffectiveControlWorldTick;
        }
    }

    if (bHasEffectiveControl)
    {
        State.bHasLastEffectiveControlTick = true;
        State.LastEffectiveControlWorldTick =
            WorldTick;
    }

    if (State.CurrentTitle ==
            FName(TEXT("overlord")) ||
        (bExistingFound &&
         Existing.HistoricalPeakTitle ==
            FName(TEXT("overlord"))))
    {
        State.HistoricalPeakTitle =
            FName(TEXT("overlord"));
    }
    else if (State.CurrentTitle ==
                 FName(TEXT("ruler")) ||
             (bExistingFound &&
              Existing.HistoricalPeakTitle ==
                 FName(TEXT("ruler"))))
    {
        State.HistoricalPeakTitle =
            FName(TEXT("ruler"));
    }
    else
    {
        State.HistoricalPeakTitle =
            FName(TEXT("none"));
    }

    if (!Store.UpsertRulerSovereigntyState(
            State,
            OutError))
    {
        return false;
    }

    OutState = State;
    return true;
}
