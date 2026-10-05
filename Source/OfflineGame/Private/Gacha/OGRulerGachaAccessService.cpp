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
    bool bHasEffectiveTerritoryControl,
    bool bInReclamationGrace,
    bool bMoreThanOneInGameMonthElapsed,
    FOGRulerGachaAccessRecord& OutAccess,
    FString& OutError)
{
    OutAccess = FOGRulerGachaAccessRecord();
    OutError.Reset();

    if (!RulerId.IsValid() ||
        WorldTick < 0 ||
        (bHasEffectiveTerritoryControl &&
         bInReclamationGrace))
    {
        OutError =
            TEXT("Gacha qualification refresh received invalid Ruler/time/control state.");
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
                WorldTick;
        }

        // Reclamation inside the fixed five-day grace preserves continuity.
        Access.bHasQualificationSuspendedTick = false;
        Access.QualificationSuspendedWorldTick = 0;

        if (bMoreThanOneInGameMonthElapsed)
        {
            if (!Access.bHasQualificationStart)
            {
                OutError =
                    TEXT("Cannot unlock gacha without a qualification start.");
                return false;
            }

            Access.bPermanentlyUnlocked = true;
            Access.bHasUnlockedWorldTick = true;
            Access.UnlockedWorldTick =
                WorldTick;
        }
    }
    else if (bInReclamationGrace)
    {
        if (Access.bHasQualificationStart &&
            !Access.bHasQualificationSuspendedTick)
        {
            Access.bHasQualificationSuspendedTick = true;
            Access.QualificationSuspendedWorldTick =
                WorldTick;
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
             !bHasEffectiveTerritoryControl &&
             !bInReclamationGrace)
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
