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

    FOGWarParticipantHistoryRecord AttackerHistory;
    AttackerHistory.WarId = War.WarId;
    AttackerHistory.FactionId = AttackerFaction;
    AttackerHistory.SideIndex = 0;
    AttackerHistory.JoinedWorldTick = WorldTick;

    FOGWarParticipantHistoryRecord DefenderHistory;
    DefenderHistory.WarId = War.WarId;
    DefenderHistory.FactionId = DefenderFaction;
    DefenderHistory.SideIndex = 1;
    DefenderHistory.JoinedWorldTick = WorldTick;

    if (!Store.UpsertWarParticipantHistory(
            AttackerHistory,
            OutError) ||
        !Store.UpsertWarParticipantHistory(
            DefenderHistory,
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


bool FOGFactionWarService::CreateWarFront(
    const FOGEntityId& WarId,
    const FOGEntityId& LocationId,
    const FOGEntityId& RealityId,
    int64 WorldTick,
    const FString& StateJson,
    FOGEntityId& OutFrontId,
    FString& OutError)
{
    OutFrontId =
        FOGEntityId();
    OutError.Reset();

    if (!WarId.IsValid() ||
        (!LocationId.IsValid() &&
         !RealityId.IsValid()) ||
        WorldTick < 0)
    {
        OutError =
            TEXT("War front creation request is invalid.");
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
        OutError =
            TEXT("War front requires an active parent War.");
        return false;
    }

    FOGWarFrontRecord Front;
    Front.FrontId =
        FOGEntityId::NewId();
    Front.WarId =
        WarId;
    Front.LocationId =
        LocationId;
    Front.RealityId =
        RealityId;
    Front.State =
        FName(TEXT("active"));
    Front.StartWorldTick =
        WorldTick;
    Front.StateJson =
        StateJson.IsEmpty()
            ? TEXT("{}")
            : StateJson;

    if (!Store.UpsertWarFront(
            Front,
            WorldTick,
            OutError))
    {
        return false;
    }

    OutFrontId =
        Front.FrontId;
    return true;
}

bool FOGFactionWarService::SetWarObjective(
    const FOGWarObjectiveRecord& Objective,
    FString& OutError)
{
    bool bFound = false;
    FOGWarRecord War;
    if (!Store.TryReadWar(
            Objective.WarId,
            bFound,
            War,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("War objective requires an existing parent War.");
        return false;
    }

    if (Objective.FrontId.IsValid())
    {
        FOGWarFrontRecord Front;
        bool bFrontFound = false;
        if (!Store.TryReadWarFront(
                Objective.FrontId,
                bFrontFound,
                Front,
                OutError))
        {
            return false;
        }

        if (!bFrontFound ||
            Front.WarId !=
                Objective.WarId)
        {
            OutError =
                TEXT("War objective front does not belong to the parent War.");
            return false;
        }
    }

    return Store.UpsertWarObjective(
        Objective,
        OutError);
}

bool FOGFactionWarService::IssueWarOrder(
    const FOGEntityId& WarId,
    const FOGEntityId& FrontId,
    const FOGEntityId& IssuerEntityId,
    const FOGEntityId& RecipientEntityId,
    const FOGContentId& IntentId,
    const FString& ConstraintsJson,
    int64 WorldTick,
    FOGEntityId& OutOrderId,
    FString& OutError)
{
    OutOrderId =
        FOGEntityId();
    OutError.Reset();

    if (!WarId.IsValid() ||
        !IssuerEntityId.IsValid() ||
        !RecipientEntityId.IsValid() ||
        !IntentId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("War order request is invalid.");
        return false;
    }

    bool bWarFound = false;
    FOGWarRecord War;
    if (!Store.TryReadWar(
            WarId,
            bWarFound,
            War,
            OutError))
    {
        return false;
    }

    if (!bWarFound ||
        War.Status !=
            EOGWarStatus::Active)
    {
        OutError =
            TEXT("War order requires an active War.");
        return false;
    }

    if (FrontId.IsValid())
    {
        bool bFrontFound = false;
        FOGWarFrontRecord Front;
        if (!Store.TryReadWarFront(
                FrontId,
                bFrontFound,
                Front,
                OutError))
        {
            return false;
        }

        if (!bFrontFound ||
            Front.WarId != WarId)
        {
            OutError =
                TEXT("War order front does not belong to the parent War.");
            return false;
        }
    }

    FOGWarOrderRecord Order;
    Order.OrderId =
        FOGEntityId::NewId();
    Order.WarId =
        WarId;
    Order.FrontId =
        FrontId;
    Order.IssuerEntityId =
        IssuerEntityId;
    Order.RecipientEntityId =
        RecipientEntityId;
    Order.IntentId =
        IntentId;
    Order.ConstraintsJson =
        ConstraintsJson.IsEmpty()
            ? TEXT("{}")
            : ConstraintsJson;
    Order.IssuedWorldTick =
        WorldTick;
    Order.OutcomeState =
        FName(TEXT("pending"));

    if (!Store.UpsertWarOrder(
            Order,
            WorldTick,
            OutError))
    {
        return false;
    }

    OutOrderId =
        Order.OrderId;
    return true;
}

bool FOGFactionWarService::RecordWarOrderOutcome(
    const FOGEntityId& OrderId,
    FName OutcomeState,
    const FString& OutcomeJson,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!OrderId.IsValid() ||
        OutcomeState.IsNone() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("War order outcome request is invalid.");
        return false;
    }

    bool bFound = false;
    FOGWarOrderRecord Order;
    if (!Store.TryReadWarOrder(
            OrderId,
            bFound,
            Order,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("War order does not exist.");
        return false;
    }

    Order.OutcomeState =
        OutcomeState;
    Order.OutcomeJson =
        OutcomeJson.IsEmpty()
            ? TEXT("{}")
            : OutcomeJson;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertWarOrder(
            Order,
            Order.IssuedWorldTick,
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
        FName(TEXT("war.order_outcome"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        Order.OrderId;
    Event.RelatedEntities.Add(
        Order.WarId);
    Event.RelatedEntities.Add(
        Order.RecipientEntityId);
    Event.bChronicleEligible =
        OutcomeState ==
            FName(TEXT("disobeyed")) ||
        OutcomeState ==
            FName(TEXT("refused"));
    Event.PayloadJson = FString::Printf(
        TEXT("{\"outcome\":\"%s\",\"details\":%s}"),
        *OutcomeState.ToString(),
        *Order.OutcomeJson);

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

bool FOGFactionWarService::JoinWar(
    const FOGEntityId& WarId,
    const FOGEntityId& FactionId,
    int32 SideIndex,
    bool bPrimary,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!WarId.IsValid() ||
        !FactionId.IsValid() ||
        SideIndex < 0 ||
        WorldTick < 0)
    {
        OutError =
            TEXT("War join request is invalid.");
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
        OutError =
            TEXT("Faction can join only an active War.");
        return false;
    }

    for (const FOGWarParticipant& Participant :
         War.Participants)
    {
        if (Participant.FactionId ==
            FactionId)
        {
            OutError =
                TEXT("Faction is already an active War participant.");
            return false;
        }
    }

    FOGWarParticipant Participant;
    Participant.FactionId =
        FactionId;
    Participant.SideIndex =
        SideIndex;
    Participant.bPrimary =
        bPrimary;
    War.Participants.Add(
        Participant);

    FOGWarParticipantHistoryRecord History;
    History.WarId =
        WarId;
    History.FactionId =
        FactionId;
    History.SideIndex =
        SideIndex;
    History.JoinedWorldTick =
        WorldTick;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertWar(
            War,
            War.StartWorldTick,
            OutError) ||
        !Store.UpsertWarParticipantHistory(
            History,
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

bool FOGFactionWarService::LeaveWar(
    const FOGEntityId& WarId,
    const FOGEntityId& FactionId,
    FName Reason,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!WarId.IsValid() ||
        !FactionId.IsValid() ||
        Reason.IsNone() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("War leave request is invalid.");
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
        OutError =
            TEXT("Faction can leave only an active War.");
        return false;
    }

    int32 RemoveIndex = INDEX_NONE;
    for (int32 Index = 0;
         Index < War.Participants.Num();
         ++Index)
    {
        if (War.Participants[Index].FactionId ==
            FactionId)
        {
            RemoveIndex =
                Index;
            break;
        }
    }

    if (RemoveIndex == INDEX_NONE)
    {
        OutError =
            TEXT("Faction is not an active participant in this War.");
        return false;
    }

    TArray<FOGWarParticipantHistoryRecord> History;
    if (!Store.ListWarParticipantHistory(
            WarId,
            History,
            OutError))
    {
        return false;
    }

    FOGWarParticipantHistoryRecord* OpenHistory = nullptr;
    for (FOGWarParticipantHistoryRecord& Row :
         History)
    {
        if (Row.FactionId ==
                FactionId &&
            !Row.bHasLeftWorldTick)
        {
            OpenHistory =
                &Row;
        }
    }

    if (!OpenHistory)
    {
        OutError =
            TEXT("Active War participant has no open participation history.");
        return false;
    }

    OpenHistory->bHasLeftWorldTick =
        true;
    OpenHistory->LeftWorldTick =
        WorldTick;
    OpenHistory->Reason =
        Reason;

    War.Participants.RemoveAt(
        RemoveIndex);

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertWar(
            War,
            War.StartWorldTick,
            OutError) ||
        !Store.UpsertWarParticipantHistory(
            *OpenHistory,
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
