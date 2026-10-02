#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGDispatchFactionWarRecords.h"

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

    /**
     * Resolution happens once when relevant/due. The result is supplied by the
     * capability/mission resolver; there is no hidden step-by-step NPC sim.
     */
    bool ResolveDispatch(
        const FOGEntityId& DispatchId,
        int64 CurrentWorldTick,
        bool bSucceeded,
        const FString& ResultJson,
        FOGDispatchRecord& OutDispatch,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
