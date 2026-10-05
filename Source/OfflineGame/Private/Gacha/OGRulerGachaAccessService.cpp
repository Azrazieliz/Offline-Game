#include "Gacha/OGRulerGachaAccessService.h"

#include "Events/OGWorldEvent.h"

bool FOGRulerGachaAccessService::CanUseGacha(
    const FOGEntityId& RulerId,
    int64 WorldTick,
    bool& bOutCanUse,
    FString& OutError) const
{
    bOutCanUse = false;
    OutError.Reset();

    if (!RulerId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Gacha-access check requires a valid Ruler and non-negative world tick.");
        return false;
    }

    bool bFound = false;
    FOGRulerGachaAccessRecord Access;
    if (!Store.TryReadRulerGachaAccess(
            RulerId,
            bFound,
            Access,
            OutError))
    {
        return false;
    }

    bOutCanUse =
        bFound &&
        Access.bPermanentlyUnlocked;
    return true;
}

bool FOGRulerGachaAccessService::RefreshGachaQualification(
    const FOGEntityId& RulerId,
    int64 WorldTick,
    bool bMoreThanOneInGameMonthElapsed,
    FOGRulerGachaAccessRecord& OutAccess,
    FString& OutError)
{
    OutAccess = FOGRulerGachaAccessRecord();
    OutError.Reset();

    if (!RulerId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Gacha qualification refresh requires a valid Ruler and non-negative world tick.");
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

    bool bHasEffectiveTerritoryControl = false;
    bool bInReclamationGrace = false;
    bool bHaveContinuityStart = false;
    int64 EarliestContinuityStart = 0;
    bool bHaveDisplacementTick = false;
    int64 EarliestDisplacementTick = 0;

    for (const FOGTerritoryClaimRecord& Claim :
         Claims)
    {
        const bool bEffective =
            Claim.ControlState ==
                FName(TEXT("controlled")) ||
            Claim.ControlState ==
                FName(TEXT("effective"));
        const bool bGrace =
            Claim.ControlState ==
                FName(TEXT("displaced")) &&
            Claim.bHasReclaimDeadline &&
            WorldTick <=
                Claim.ReclaimDeadlineWorldTick;

        if (!bEffective &&
            !bGrace)
        {
            continue;
        }

        bHasEffectiveTerritoryControl |=
            bEffective;
        bInReclamationGrace |=
            bGrace;

        const int64 ContinuityStart =
            Claim.bHasEffectiveControlStart
                ? Claim.EffectiveControlStartWorldTick
                : Claim.ClaimStartWorldTick;

        if (!bHaveContinuityStart ||
            ContinuityStart <
                EarliestContinuityStart)
        {
            bHaveContinuityStart = true;
            EarliestContinuityStart =
                ContinuityStart;
        }

        if (bGrace &&
            Claim.bHasDisplacedWorldTick)
        {
            if (!bHaveDisplacementTick ||
                Claim.DisplacedWorldTick <
                    EarliestDisplacementTick)
            {
                bHaveDisplacementTick = true;
                EarliestDisplacementTick =
                    Claim.DisplacedWorldTick;
            }
        }
    }

    // Any still-effective Territory keeps qualification actively running even
    // if another holding is inside reclamation grace.
    if (bHasEffectiveTerritoryControl)
    {
        bInReclamationGrace = false;
    }

    bool bFound = false;
    FOGRulerGachaAccessRecord Access;
    if (!Store.TryReadRulerGachaAccess(
            RulerId,
            bFound,
            Access,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        Access.RulerId = RulerId;
    }

    // Permanent means permanent. Later Territory loss can never revoke access.
    if (Access.bPermanentlyUnlocked)
    {
        Access.UpdatedWorldTick =
            FMath::Max(
                Access.UpdatedWorldTick,
                WorldTick);

        if (!Store.UpsertRulerGachaAccess(
                Access,
                OutError))
        {
            return false;
        }

        OutAccess = Access;
        return true;
    }

    const bool bHadQualificationStart =
        Access.bHasQualificationStart;

    if (bHasEffectiveTerritoryControl)
    {
        if (!Access.bHasQualificationStart)
        {
            Access.bHasQualificationStart = true;
            Access.QualificationStartWorldTick =
                bHaveContinuityStart
                    ? EarliestContinuityStart
                    : WorldTick;
        }

        // Reclamation inside the fixed five-day grace preserves continuity.
        Access.bHasQualificationSuspendedTick = false;
        Access.QualificationSuspendedWorldTick = 0;

        if (bMoreThanOneInGameMonthElapsed)
        {
            Access.bPermanentlyUnlocked = true;
            Access.bHasUnlockedWorldTick = true;
            Access.UnlockedWorldTick =
                WorldTick;
        }
    }
    else if (bInReclamationGrace)
    {
        // If no qualification row was projected before displacement, recover
        // its continuity origin from the persisted claim rather than losing
        // legitimate elapsed time.
        if (!Access.bHasQualificationStart &&
            bHaveContinuityStart)
        {
            Access.bHasQualificationStart = true;
            Access.QualificationStartWorldTick =
                EarliestContinuityStart;
        }

        if (Access.bHasQualificationStart &&
            !Access.bHasQualificationSuspendedTick)
        {
            Access.bHasQualificationSuspendedTick = true;
            Access.QualificationSuspendedWorldTick =
                bHaveDisplacementTick
                    ? EarliestDisplacementTick
                    : WorldTick;
        }
    }
    else
    {
        // A true continuity break before first unlock resets qualification.
        Access.bHasQualificationStart = false;
        Access.QualificationStartWorldTick = 0;
        Access.bHasQualificationSuspendedTick = false;
        Access.QualificationSuspendedWorldTick = 0;
    }

    Access.UpdatedWorldTick =
        WorldTick;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertRulerGachaAccess(
            Access,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    const bool bJustUnlocked =
        Access.bPermanentlyUnlocked;

    if (bJustUnlocked)
    {
        FOGWorldEvent Event;
        Event.EventId = FOGEntityId::NewId();
        Event.EventType =
            FName(TEXT("gacha.permanently_unlocked"));
        Event.WorldTick = WorldTick;
        Event.PrimaryEntity = RulerId;
        Event.bChronicleEligible = true;
        Event.PayloadJson =
            TEXT("{\"qualification\":\"more_than_one_in_game_month_continuous_territory_control\"}");

        if (!Store.AppendWorldEvent(
                Event,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }
    else if (bHadQualificationStart &&
             !Access.bHasQualificationStart)
    {
        FOGWorldEvent Event;
        Event.EventId = FOGEntityId::NewId();
        Event.EventType =
            FName(TEXT("gacha.qualification_reset"));
        Event.WorldTick = WorldTick;
        Event.PrimaryEntity = RulerId;
        Event.bChronicleEligible = false;

        if (!Store.AppendWorldEvent(
                Event,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    OutAccess = Access;
    return true;
}
