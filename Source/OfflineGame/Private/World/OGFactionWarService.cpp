#include "World/OGFactionWarService.h"

#include "Events/OGWorldEvent.h"

void FOGFactionWarService::CanonicalizeAlliancePair(
    FOGEntityId& InOutA,
    FOGEntityId& InOutB)
{
    if (InOutA.ToString().Compare(
            InOutB.ToString(),
            ESearchCase::CaseSensitive) > 0)
    {
        Swap(
            InOutA,
            InOutB);
    }
}

bool FOGFactionWarService::FormAlliance(
    const FOGEntityId& FactionA,
    const FOGEntityId& FactionB,
    int64 WorldTick,
    FString& OutError)
{
    FOGEntityId A = FactionA;
    FOGEntityId B = FactionB;
    CanonicalizeAlliancePair(A, B);

    if (!A.IsValid() ||
        !B.IsValid() ||
        A == B)
    {
        OutError = TEXT("Alliance requires two distinct valid factions.");
        return false;
    }

    FOGFactionLinkRecord Link;
    Link.SourceFactionId = A;
    Link.TargetFactionId = B;
    Link.Type = EOGFactionLinkType::Alliance;
    Link.bActive = true;
    Link.UpdatedWorldTick = WorldTick;

    return Store.UpsertFactionLink(
        Link,
        OutError);
}

bool FOGFactionWarService::BreakAlliance(
    const FOGEntityId& FactionA,
    const FOGEntityId& FactionB,
    int64 WorldTick,
    FString& OutError)
{
    FOGEntityId A = FactionA;
    FOGEntityId B = FactionB;
    CanonicalizeAlliancePair(A, B);

    if (!A.IsValid() ||
        !B.IsValid() ||
        A == B)
    {
        OutError = TEXT("Alliance break requires two distinct valid factions.");
        return false;
    }

    FOGFactionLinkRecord Link;
    Link.SourceFactionId = A;
    Link.TargetFactionId = B;
    Link.Type = EOGFactionLinkType::Alliance;
    Link.bActive = false;
    Link.UpdatedWorldTick = WorldTick;

    return Store.UpsertFactionLink(
        Link,
        OutError);
}

bool FOGFactionWarService::SetSupport(
    const FOGEntityId& SupportingFaction,
    const FOGEntityId& SupportedFaction,
    bool bActive,
    int64 WorldTick,
    FString& OutError)
{
    if (!SupportingFaction.IsValid() ||
        !SupportedFaction.IsValid() ||
        SupportingFaction ==
            SupportedFaction)
    {
        OutError = TEXT("Support requires two distinct valid factions.");
        return false;
    }

    FOGFactionLinkRecord Link;
    Link.SourceFactionId =
        SupportingFaction;
    Link.TargetFactionId =
        SupportedFaction;
    Link.Type =
        EOGFactionLinkType::Support;
    Link.bActive =
        bActive;
    Link.UpdatedWorldTick =
        WorldTick;

    return Store.UpsertFactionLink(
        Link,
        OutError);
}

bool FOGFactionWarService::SetSubordination(
    const FOGEntityId& SubordinateFaction,
    const FOGEntityId& OverlordFaction,
    bool bActive,
    int64 WorldTick,
    FString& OutError)
{
    if (!SubordinateFaction.IsValid() ||
        !OverlordFaction.IsValid() ||
        SubordinateFaction ==
            OverlordFaction)
    {
        OutError = TEXT("Subordination requires two distinct valid factions.");
        return false;
    }

    FOGFactionLinkRecord Link;
    Link.SourceFactionId =
        SubordinateFaction;
    Link.TargetFactionId =
        OverlordFaction;
    Link.Type =
        EOGFactionLinkType::Subordination;
    Link.bActive =
        bActive;
    Link.UpdatedWorldTick =
        WorldTick;

    return Store.UpsertFactionLink(
        Link,
        OutError);
}

bool FOGFactionWarService::DeclareWar(
    const FOGEntityId& AttackerFaction,
    const FOGEntityId& DefenderFaction,
    FName ObjectiveType,
    const FOGEntityId& ObjectiveTargetEntityId,
    int64 WorldTick,
    FOGEntityId& OutWarId,
    FString& OutError)
{
    OutWarId = FOGEntityId();
    OutError.Reset();

    if (!AttackerFaction.IsValid() ||
        !DefenderFaction.IsValid() ||
        AttackerFaction ==
            DefenderFaction ||
        ObjectiveType.IsNone())
    {
        OutError = TEXT("War declaration is invalid.");
        return false;
    }

    if (!Store.BeginTransaction(OutError))
    {
        return false;
    }

    FOGEntityId AllianceA =
        AttackerFaction;
    FOGEntityId AllianceB =
        DefenderFaction;
    CanonicalizeAlliancePair(
        AllianceA,
        AllianceB);

    bool bAllianceKnown = false;
    FOGFactionLinkRecord ExistingAlliance;

    if (!Store.TryReadFactionLink(
            AllianceA,
            AllianceB,
            EOGFactionLinkType::Alliance,
            bAllianceKnown,
            ExistingAlliance,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    if (bAllianceKnown &&
        ExistingAlliance.bActive)
    {
        ExistingAlliance.bActive = false;
        ExistingAlliance.UpdatedWorldTick =
            WorldTick;

        if (!Store.UpsertFactionLink(
                ExistingAlliance,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    FOGWarRecord War;
    War.WarId =
        FOGEntityId::NewId();
    War.Status =
        EOGWarStatus::Active;
    War.ObjectiveType =
        ObjectiveType;
    War.ObjectiveTargetEntityId =
        ObjectiveTargetEntityId;
    War.StartWorldTick =
        WorldTick;

    FOGWarParticipant Attacker;
    Attacker.FactionId =
        AttackerFaction;
    Attacker.SideIndex = 0;
    Attacker.bPrimary = true;

    FOGWarParticipant Defender;
    Defender.FactionId =
        DefenderFaction;
    Defender.SideIndex = 1;
    Defender.bPrimary = true;

    War.Participants =
    {
        Attacker,
        Defender
    };

    if (!Store.UpsertWar(
            War,
            WorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        TEXT("war.began");
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        War.WarId;
    Event.RelatedEntities.Add(
        AttackerFaction);
    Event.RelatedEntities.Add(
        DefenderFaction);
    Event.bChronicleEligible =
        true;

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

    OutWarId =
        War.WarId;
    return true;
}

bool FOGFactionWarService::ResolveWar(
    const FOGEntityId& WarId,
    EOGWarStatus FinalStatus,
    int64 WorldTick,
    const FString& ResolutionJson,
    FString& OutError)
{
    OutError.Reset();

    if (FinalStatus ==
        EOGWarStatus::Active)
    {
        OutError = TEXT("War resolution requires a non-active final status.");
        return false;
    }

    bool bFound = false;
    FOGWarRecord War;

    if (!Store.TryReadWar(
            WarId,
            bFound,
            War,
            OutError))
    {
        return false;
    }

    if (!bFound ||
        War.Status !=
            EOGWarStatus::Active)
    {
        OutError = TEXT("Active war was not found.");
        return false;
    }

    War.Status =
        FinalStatus;
    War.EndWorldTick =
        WorldTick;
    War.ResolutionJson =
        ResolutionJson.IsEmpty()
            ? TEXT("{}")
            : ResolutionJson;

    if (!Store.BeginTransaction(OutError))
    {
        return false;
    }

    if (!Store.UpsertWar(
            War,
            War.StartWorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        TEXT("war.ended");
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        War.WarId;
    Event.bChronicleEligible =
        true;

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
