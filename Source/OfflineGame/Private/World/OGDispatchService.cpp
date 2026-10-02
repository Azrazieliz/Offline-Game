#include "World/OGDispatchService.h"

bool FOGDispatchService::StartDispatch(
    const FOGEntityId& OwnerEntityId,
    const FOGEntityId& TargetEntityId,
    EOGDispatchType Type,
    const TArray<FOGEntityId>& ParticipantEntityIds,
    int64 StartWorldTick,
    int64 ResolveWorldTick,
    int32 RiskBps,
    int64 ResolutionSeed,
    FOGEntityId& OutDispatchId,
    FString& OutError)
{
    OutDispatchId = FOGEntityId();
    OutError.Reset();

    if (!OwnerEntityId.IsValid() ||
        ResolveWorldTick < StartWorldTick ||
        RiskBps < 0 ||
        RiskBps > 10000)
    {
        OutError = TEXT("Dispatch definition is invalid.");
        return false;
    }

    TSet<FOGEntityId> UniqueParticipants;
    for (const FOGEntityId& ParticipantId :
         ParticipantEntityIds)
    {
        if (!ParticipantId.IsValid() ||
            UniqueParticipants.Contains(
                ParticipantId))
        {
            OutError = TEXT("Dispatch contains invalid or duplicate participants.");
            return false;
        }

        UniqueParticipants.Add(
            ParticipantId);
    }

    FOGDispatchRecord Dispatch;
    Dispatch.DispatchId =
        FOGEntityId::NewId();
    Dispatch.OwnerEntityId =
        OwnerEntityId;
    Dispatch.TargetEntityId =
        TargetEntityId;
    Dispatch.Type =
        Type;
    Dispatch.Status =
        EOGDispatchStatus::Active;
    Dispatch.ParticipantEntityIds =
        ParticipantEntityIds;
    Dispatch.StartWorldTick =
        StartWorldTick;
    Dispatch.ResolveWorldTick =
        ResolveWorldTick;
    Dispatch.RiskBps =
        RiskBps;
    Dispatch.ResolutionSeed =
        ResolutionSeed;

    if (!Store.UpsertDispatch(
            Dispatch,
            StartWorldTick,
            OutError))
    {
        return false;
    }

    OutDispatchId =
        Dispatch.DispatchId;
    return true;
}

bool FOGDispatchService::ResolveDispatch(
    const FOGEntityId& DispatchId,
    int64 CurrentWorldTick,
    bool bSucceeded,
    const FString& ResultJson,
    FOGDispatchRecord& OutDispatch,
    FString& OutError)
{
    OutDispatch =
        FOGDispatchRecord();
    OutError.Reset();

    bool bFound = false;
    if (!Store.TryReadDispatch(
            DispatchId,
            bFound,
            OutDispatch,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError = TEXT("Dispatch does not exist.");
        return false;
    }

    if (OutDispatch.Status !=
        EOGDispatchStatus::Active)
    {
        OutError = TEXT("Dispatch is not active.");
        return false;
    }

    if (CurrentWorldTick <
        OutDispatch.ResolveWorldTick)
    {
        OutError = TEXT("Dispatch has not reached its resolution tick.");
        return false;
    }

    OutDispatch.Status =
        bSucceeded
            ? EOGDispatchStatus::Succeeded
            : EOGDispatchStatus::Failed;

    OutDispatch.ResultJson =
        ResultJson.IsEmpty()
            ? TEXT("{}")
            : ResultJson;

    return Store.UpsertDispatch(
        OutDispatch,
        OutDispatch.StartWorldTick,
        OutError);
}
