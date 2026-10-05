#include "World/OGRealityGraphService.h"

#include "Dom/JsonObject.h"
#include "Events/OGWorldEvent.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool FOGRealityGraphService::SaveRealityNode(
    const FOGRealityNodeRecord& Reality,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Reality.RealityId.IsValid() ||
        Reality.Kind.IsNone() ||
        CreatedWorldTick < 0)
    {
        OutError =
            TEXT("Reality-node save request is invalid.");
        return false;
    }

    if (Reality.TimeDomainId.IsValid())
    {
        bool bTimeFound = false;
        FOGTimeDomainRecord TimeDomain;
        if (!Store.TryReadTimeDomain(
                Reality.TimeDomainId,
                bTimeFound,
                TimeDomain,
                OutError))
        {
            return false;
        }

        if (!bTimeFound)
        {
            OutError =
                TEXT("Reality node references an unknown Time Domain.");
            return false;
        }
    }

    if (Reality.ParentRealityId.IsValid())
    {
        TSet<FOGEntityId> Visited;
        FOGEntityId Cursor =
            Reality.ParentRealityId;

        while (Cursor.IsValid())
        {
            if (Cursor ==
                Reality.RealityId)
            {
                OutError =
                    TEXT("Reality hierarchy would create a cycle.");
                return false;
            }

            if (Visited.Contains(Cursor))
            {
                OutError =
                    TEXT("Existing Reality hierarchy already contains a cycle.");
                return false;
            }

            Visited.Add(Cursor);

            bool bFound = false;
            FOGRealityNodeRecord Parent;
            if (!Store.TryReadRealityNode(
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
                    TEXT("Reality parent does not exist.");
                return false;
            }

            Cursor =
                Parent.ParentRealityId;
        }
    }

    return Store.UpsertRealityNode(
        Reality,
        CreatedWorldTick,
        OutError);
}

bool FOGRealityGraphService::SaveJunction(
    const FOGJunctionRecord& Junction,
    int64 CreatedWorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (!Junction.JunctionId.IsValid() ||
        !Junction.FromRealityId.IsValid() ||
        !Junction.ToRealityId.IsValid() ||
        Junction.FromRealityId ==
            Junction.ToRealityId ||
        CreatedWorldTick < 0)
    {
        OutError =
            TEXT("Junction save request is invalid.");
        return false;
    }

    bool bFromFound = false;
    bool bToFound = false;
    FOGRealityNodeRecord From;
    FOGRealityNodeRecord To;

    if (!Store.TryReadRealityNode(
            Junction.FromRealityId,
            bFromFound,
            From,
            OutError) ||
        !Store.TryReadRealityNode(
            Junction.ToRealityId,
            bToFound,
            To,
            OutError))
    {
        return false;
    }

    if (!bFromFound ||
        !bToFound)
    {
        OutError =
            TEXT("Junction endpoints must be persistent Reality nodes.");
        return false;
    }

    return Store.UpsertJunction(
        Junction,
        CreatedWorldTick,
        OutError);
}

bool FOGRealityGraphService::SetWorldRank(
    const FOGEntityId& RealityId,
    const FOGContentId& NewWorldRankId,
    int64 WorldTick,
    const FString& ProvenanceJson,
    FString& OutError)
{
    OutError.Reset();

    if (!RealityId.IsValid() ||
        !NewWorldRankId.IsValid() ||
        WorldTick < 0 ||
        ProvenanceJson.IsEmpty() ||
        ProvenanceJson == TEXT("{}"))
    {
        OutError =
            TEXT("World Rank change requires valid Reality, content Rank and provenance.");
        return false;
    }

    TSharedPtr<FJsonObject> Provenance;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(
            ProvenanceJson);
    if (!FJsonSerializer::Deserialize(
            Reader,
            Provenance) ||
        !Provenance.IsValid())
    {
        OutError =
            TEXT("World Rank provenance must be a valid JSON object.");
        return false;
    }

    bool bFound = false;
    FOGRealityNodeRecord Reality;
    if (!Store.TryReadRealityNode(
            RealityId,
            bFound,
            Reality,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot change World Rank for an unknown Reality.");
        return false;
    }

    const FOGContentId PreviousRank =
        Reality.WorldRankId;
    Reality.WorldRankId =
        NewWorldRankId;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertRealityNode(
            Reality,
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
        FName(TEXT("reality.world_rank_changed"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        RealityId;
    Event.bChronicleEligible =
        true;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"previous\":\"%s\",\"current\":\"%s\",\"provenance\":%s}"),
        *PreviousRank.ToString(),
        *NewWorldRankId.ToString(),
        *ProvenanceJson);

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

bool FOGRealityGraphService::OpenJunction(
    const FOGEntityId& JunctionId,
    int64 WorldTick,
    bool bRequirementsSatisfied,
    FString& OutError)
{
    OutError.Reset();

    if (!JunctionId.IsValid() ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Junction open request is invalid.");
        return false;
    }

    if (!bRequirementsSatisfied)
    {
        OutError =
            TEXT("Junction requirements are not satisfied.");
        return false;
    }

    bool bFound = false;
    FOGJunctionRecord Junction;
    if (!Store.TryReadJunction(
            JunctionId,
            bFound,
            Junction,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot open an unknown Junction.");
        return false;
    }

    if (Junction.State ==
        FName(TEXT("open")))
    {
        return true;
    }

    Junction.State =
        FName(TEXT("open"));
    Junction.bHasOpenedWorldTick =
        true;
    Junction.OpenedWorldTick =
        WorldTick;
    Junction.bHasClosedWorldTick =
        false;
    Junction.ClosedWorldTick = 0;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertJunction(
            Junction,
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
        FName(TEXT("reality.junction_opened"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        Junction.JunctionId;
    Event.RelatedEntities.Add(
        Junction.FromRealityId);
    Event.RelatedEntities.Add(
        Junction.ToRealityId);
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

bool FOGRealityGraphService::CloseJunction(
    const FOGEntityId& JunctionId,
    int64 WorldTick,
    FName Reason,
    FString& OutError)
{
    OutError.Reset();

    if (!JunctionId.IsValid() ||
        WorldTick < 0 ||
        Reason.IsNone())
    {
        OutError =
            TEXT("Junction close request is invalid.");
        return false;
    }

    bool bFound = false;
    FOGJunctionRecord Junction;
    if (!Store.TryReadJunction(
            JunctionId,
            bFound,
            Junction,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot close an unknown Junction.");
        return false;
    }

    Junction.State =
        FName(TEXT("closed"));
    Junction.bHasClosedWorldTick =
        true;
    Junction.ClosedWorldTick =
        WorldTick;

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertJunction(
            Junction,
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
        FName(TEXT("reality.junction_closed"));
    Event.WorldTick =
        WorldTick;
    Event.PrimaryEntity =
        Junction.JunctionId;
    Event.bChronicleEligible =
        true;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"reason\":\"%s\"}"),
        *Reason.ToString());

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
