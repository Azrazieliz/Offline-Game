#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGDispatchFactionWarRecords.h"
#include "World/OGStrategyExpansionRecords.h"

class OFFLINEGAME_API FOGDispatchService
{
public:
    explicit FOGDispatchService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool StartDispatch(
        const FOGEntityId& OwnerEntityId,
        const FOGEntityId& TargetEntityId,
        EOGDispatchType Type,
        const TArray<FOGEntityId>& ParticipantEntityIds,
        int64 StartWorldTick,
        int64 ResolveWorldTick,
        int32 RiskBps,
        int64 ResolutionSeed,
        FOGEntityId& OutDispatchId,
        FString& OutError);

    bool StartDispatch(
        const FOGEntityId& OwnerEntityId,
        const FOGEntityId& TargetEntityId,
        EOGDispatchType Type,
        const TArray<FOGEntityId>& ParticipantEntityIds,
        int64 StartWorldTick,
        int64 ResolveWorldTick,
        int32 RiskBps,
        int32 RiskToleranceBps,
        const FString& AbortPolicyJson,
        const TArray<FOGDispatchObjectiveRecord>& Objectives,
        const TArray<FOGDispatchConstraintRecord>& Constraints,
        int64 ResolutionSeed,
        FOGEntityId& OutDispatchId,
        FString& OutError);

    /**
     * Compatibility finalizer for simple content. Rich mission content should
     * use ResolveDispatchWithPolicy so mandatory objectives/constraints cannot
     * be silently bypassed.
     */
    bool ResolveDispatch(
        const FOGEntityId& DispatchId,
        int64 CurrentWorldTick,
        bool bSucceeded,
        const FString& ResultJson,
        FOGDispatchRecord& OutDispatch,
        FString& OutError);

    /**
     * Resolver contract order:
     * mandatory objective + hard constraints -> survival/abort policy ->
     * secondary objectives -> compatible opportunities.
     *
     * The runtime refuses a result unless the resolver explicitly confirms the
     * first three gates were evaluated.
     */
    bool ResolveDispatchWithPolicy(
        const FOGEntityId& DispatchId,
        int64 CurrentWorldTick,
        const FOGDispatchResolver& Resolver,
        FOGDispatchRecord& OutDispatch,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
