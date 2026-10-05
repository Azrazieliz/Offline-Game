#include "World/OGTerritoryControlService.h"

#include "Events/OGWorldEvent.h"

bool FOGTerritoryControlService::IsEffectiveState(
    FName ControlState)
{
    return ControlState == FName(TEXT("effective")) ||
        ControlState == FName(TEXT("controlled"));
}

bool FOGTerritoryControlService::EvaluateEffectiveControl(
    const FOGEntityId& TerritoryId,
    int64 WorldTick,
    FOGTerritoryEffectiveControlResult& OutResult,
    FString& OutError) const
{
    OutResult = FOGTerritoryEffectiveControlResult();
    OutResult.TerritoryId = TerritoryId;
    OutError.Reset();

    if (!TerritoryId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Effective-control evaluation requires a valid Territory ID and non-negative world tick.");
        return false;
    }

    TArray<FOGTerritoryClaimRecord> Claims;
    if (!Store.ListTerritoryClaimsByTerritory(
            TerritoryId,
            Claims,
            OutError))
    {
        return false;
    }

    TSet<FOGEntityId> EffectiveRulers;
    for (const FOGTerritoryClaimRecord& Claim :
         Claims)
    {
        if (!IsEffectiveState(
                Claim.ControlState))
        {
            continue;
        }

        OutResult.EffectiveClaimIds.Add(
            Claim.ClaimId);

        if (!EffectiveRulers.Contains(
                Claim.RulerId))
        {
            EffectiveRulers.Add(
                Claim.RulerId);
            OutResult.EffectiveRulerIds.Add(
                Claim.RulerId);
        }
    }

    OutResult.bContested =
        OutResult.EffectiveRulerIds.Num() > 1;
    return true;
}

bool FOGTerritoryControlService::BeginDisplacement(
    const FOGEntityId& ClaimId,
    int64 WorldTick,
    int64 FiveDayReclaimDeadlineWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!ClaimId.IsValid() ||
        WorldTick < 0 ||
        FiveDayReclaimDeadlineWorldTick <= WorldTick)
    {
        OutError =
            TEXT("Displacement requires a valid claim, non-negative tick, and a future five-day reclamation deadline.");
        return false;
    }

    bool bFound = false;
    FOGTerritoryClaimRecord Claim;
    if (!Store.TryReadTerritoryClaim(
            ClaimId,
            bFound,
            Claim,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot displace an unknown Territory claim.");
        return false;
    }

    if (!IsEffectiveState(
            Claim.ControlState))
    {
        OutError =
            TEXT("Only an effectively controlled Territory claim can enter displacement.");
        return false;
    }

    Claim.ControlState =
        FName(TEXT("displaced"));
    Claim.bHasDisplacedWorldTick = true;
    Claim.DisplacedWorldTick =
        WorldTick;
    Claim.bHasReclaimDeadline = true;
    Claim.ReclaimDeadlineWorldTick =
        FiveDayReclaimDeadlineWorldTick;
    Claim.UpdatedWorldTick =
        WorldTick;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertTerritoryClaim(
            Claim,
            Claim.ClaimStartWorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId = FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("territory.displaced"));
    Event.WorldTick = WorldTick;
    Event.PrimaryEntity = Claim.TerritoryId;
    Event.RelatedEntities.Add(
        Claim.ClaimId);
    Event.RelatedEntities.Add(
        Claim.RulerId);
    Event.bChronicleEligible = true;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"reclaim_deadline_world_tick\":%lld}"),
        static_cast<long long>(
            FiveDayReclaimDeadlineWorldTick));

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGTerritoryControlService::ReclaimTerritory(
    const FOGEntityId& ClaimId,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!ClaimId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Territory reclamation requires a valid claim and non-negative world tick.");
        return false;
    }

    bool bFound = false;
    FOGTerritoryClaimRecord Claim;
    if (!Store.TryReadTerritoryClaim(
            ClaimId,
            bFound,
            Claim,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot reclaim an unknown Territory claim.");
        return false;
    }

    if (Claim.ControlState !=
            FName(TEXT("displaced")) ||
        !Claim.bHasReclaimDeadline ||
        !Claim.bHasDisplacedWorldTick)
    {
        OutError =
            TEXT("Territory claim is not inside a reclamation window.");
        return false;
    }

    // "By the end of the window" means the exact deadline is still reclaimable.
    if (WorldTick >
        Claim.ReclaimDeadlineWorldTick)
    {
        OutError =
            TEXT("Territory reclamation deadline has expired.");
        return false;
    }

    Claim.ControlState =
        FName(TEXT("controlled"));
    if (!Claim.bHasEffectiveControlStart)
    {
        Claim.bHasEffectiveControlStart = true;
        Claim.EffectiveControlStartWorldTick =
            Claim.ClaimStartWorldTick;
    }

    Claim.bHasDisplacedWorldTick = false;
    Claim.DisplacedWorldTick = 0;
    Claim.bHasReclaimDeadline = false;
    Claim.ReclaimDeadlineWorldTick = 0;
    Claim.UpdatedWorldTick =
        WorldTick;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertTerritoryClaim(
            Claim,
            Claim.ClaimStartWorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId = FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("territory.reclaimed"));
    Event.WorldTick = WorldTick;
    Event.PrimaryEntity =
        Claim.TerritoryId;
    Event.RelatedEntities.Add(
        Claim.ClaimId);
    Event.RelatedEntities.Add(
        Claim.RulerId);
    Event.bChronicleEligible = true;

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    return true;
}

bool FOGTerritoryControlService::ExpireReclamationIfDue(
    const FOGEntityId& ClaimId,
    int64 WorldTick,
    bool& bOutExpired,
    FString& OutError)
{
    bOutExpired = false;
    OutError.Reset();

    if (!ClaimId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Reclamation expiry requires a valid claim and non-negative world tick.");
        return false;
    }

    bool bFound = false;
    FOGTerritoryClaimRecord Claim;
    if (!Store.TryReadTerritoryClaim(
            ClaimId,
            bFound,
            Claim,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot evaluate reclamation expiry for an unknown Territory claim.");
        return false;
    }

    if (Claim.ControlState !=
            FName(TEXT("displaced")) ||
        !Claim.bHasReclaimDeadline)
    {
        return true;
    }

    if (WorldTick <=
        Claim.ReclaimDeadlineWorldTick)
    {
        return true;
    }

    Claim.ControlState =
        FName(TEXT("lost"));
    Claim.UpdatedWorldTick =
        WorldTick;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertTerritoryClaim(
            Claim,
            Claim.ClaimStartWorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId = FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("territory.reclamation_expired"));
    Event.WorldTick = WorldTick;
    Event.PrimaryEntity =
        Claim.TerritoryId;
    Event.RelatedEntities.Add(
        Claim.ClaimId);
    Event.RelatedEntities.Add(
        Claim.RulerId);
    Event.bChronicleEligible = true;

    if (!Store.AppendWorldEvent(
            Event,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (!Store.CommitTransaction(
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    bOutExpired = true;
    return true;
}

bool FOGTerritoryControlService::ListActiveClaimsForLocation(
    const FOGEntityId& LocationId,
    TArray<FOGTerritoryClaimRecord>& OutClaims,
    FString& OutError) const
{
    return Store.ListActiveClaimsForLocation(
        LocationId,
        OutClaims,
        OutError);
}

bool FOGTerritoryControlService::FindPreferredEffectiveTerritoryForRuler(
    const FOGEntityId& RulerId,
    int64 WorldTick,
    FOGEntityId& OutTerritoryId,
    FString& OutError) const
{
    OutTerritoryId = FOGEntityId();
    OutError.Reset();

    if (!RulerId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Preferred Territory lookup requires a valid Ruler and non-negative world tick.");
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

    bool bHaveBest = false;
    bool bBestMain = false;
    int64 BestStart = 0;
    FString BestId;

    for (const FOGTerritoryClaimRecord& Claim :
         Claims)
    {
        if (!IsEffectiveState(
                Claim.ControlState))
        {
            continue;
        }

        bool bTerritoryFound = false;
        FOGTerritoryRecord Territory;
        if (!Store.TryReadTerritory(
                Claim.TerritoryId,
                bTerritoryFound,
                Territory,
                OutError))
        {
            return false;
        }

        if (!bTerritoryFound)
        {
            OutError =
                TEXT("Effective Territory claim references a missing Territory.");
            return false;
        }

        const FString CandidateId =
            Claim.TerritoryId.ToString();

        const bool bBetter =
            !bHaveBest ||
            (Territory.bMainTerritory &&
             !bBestMain) ||
            (Territory.bMainTerritory ==
                 bBestMain &&
             (Claim.ClaimStartWorldTick <
                  BestStart ||
              (Claim.ClaimStartWorldTick ==
                   BestStart &&
               CandidateId < BestId)));

        if (bBetter)
        {
            bHaveBest = true;
            bBestMain =
                Territory.bMainTerritory;
            BestStart =
                Claim.ClaimStartWorldTick;
            BestId =
                CandidateId;
            OutTerritoryId =
                Claim.TerritoryId;
        }
    }

    return true;
}
