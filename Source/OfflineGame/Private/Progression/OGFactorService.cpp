#include "Progression/OGFactorService.h"

#include "Events/OGWorldEvent.h"

bool FOGFactorService::AcquireFactor(
    FOGFactorInstanceRecord Factor,
    const TArray<FOGFactorLineageRecord>& ParentLineage,
    FString& OutError)
{
    OutError.Reset();

    if (!Factor.FactorInstanceId.IsValid())
    {
        Factor.FactorInstanceId =
            FOGEntityId::NewId();
    }

    TSet<FOGEntityId> Parents;
    for (const FOGFactorLineageRecord& Edge :
         ParentLineage)
    {
        if (Edge.ChildFactorInstanceId.IsValid() &&
            Edge.ChildFactorInstanceId !=
                Factor.FactorInstanceId)
        {
            OutError =
                TEXT("Factor lineage child does not match the acquired Factor.");
            return false;
        }

        if (!Edge.ParentFactorInstanceId.IsValid() ||
            Parents.Contains(
                Edge.ParentFactorInstanceId))
        {
            OutError =
                TEXT("Factor acquisition contains invalid or duplicate parent lineage.");
            return false;
        }

        Parents.Add(
            Edge.ParentFactorInstanceId);
    }

    if (!Store.BeginTransaction(
            OutError))
    {
        return false;
    }

    if (!Store.UpsertFactorInstance(
            Factor,
            Factor.AcquiredWorldTick,
            OutError))
    {
        FString RollbackError;
        Store.RollbackTransaction(
            RollbackError);
        return false;
    }

    for (int32 Index = 0;
         Index < ParentLineage.Num();
         ++Index)
    {
        FOGFactorLineageRecord Edge =
            ParentLineage[Index];
        Edge.ChildFactorInstanceId =
            Factor.FactorInstanceId;

        if (!Store.UpsertFactorLineage(
                Edge,
                OutError))
        {
            FString RollbackError;
            Store.RollbackTransaction(
                RollbackError);
            return false;
        }
    }

    FOGWorldEvent Event;
    Event.EventId =
        FOGEntityId::NewId();
    Event.EventType =
        FName(TEXT("factor.acquired"));
    Event.WorldTick =
        Factor.AcquiredWorldTick;
    Event.PrimaryEntity =
        Factor.OwnerEntityId;
    Event.RelatedEntities.Add(
        Factor.FactorInstanceId);
    Event.bChronicleEligible =
        true;
    Event.PayloadJson = FString::Printf(
        TEXT("{\"factor\":\"%s\",\"lineage_count\":%d}"),
        *Factor.FactorId.ToString(),
        ParentLineage.Num());

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

bool FOGFactorService::SetExpressionWeight(
    const FOGEntityId& FactorInstanceId,
    int32 ExpressionWeightBps,
    int64 WorldTick,
    FString& OutError)
{
    OutError.Reset();

    if (ExpressionWeightBps < 0 ||
        ExpressionWeightBps > 10000 ||
        WorldTick < 0)
    {
        OutError =
            TEXT("Factor expression-weight update is invalid.");
        return false;
    }

    bool bFound = false;
    FOGFactorInstanceRecord Factor;
    if (!Store.TryReadFactorInstance(
            FactorInstanceId,
            bFound,
            Factor,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Cannot update expression weight for an unknown Factor.");
        return false;
    }

    Factor.ExpressionWeightBps =
        ExpressionWeightBps;
    return Store.UpsertFactorInstance(
        Factor,
        Factor.AcquiredWorldTick,
        OutError);
}

bool FOGFactorService::ListOwnedFactors(
    const FOGEntityId& OwnerEntityId,
    TArray<FOGFactorInstanceRecord>& OutFactors,
    FString& OutError) const
{
    return Store.ListFactorInstancesByOwner(
        OwnerEntityId,
        OutFactors,
        OutError);
}
