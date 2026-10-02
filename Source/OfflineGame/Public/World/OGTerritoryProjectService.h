#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGTerritoryStateRecords.h"

/**
 * Event/lazy-resolution territory service.
 *
 * It deliberately has no worker scheduling or city-simulation loop.
 */
class OFFLINEGAME_API FOGTerritoryProjectService
{
public:
    explicit FOGTerritoryProjectService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool ApplyCoreDurabilityDamage(
        const FOGEntityId& CoreId,
        const FOGLargeNumber& Damage,
        int64 WorldTick,
        FString& OutError);

    bool CaptureIntactCore(
        const FOGEntityId& CoreId,
        const FOGEntityId& NewControllerRulerId,
        int64 WorldTick,
        FString& OutError);

    bool StartProject(
        const FOGEntityId& OwnerEntityId,
        const FOGEntityId& LocationId,
        const FOGContentId& ProjectTypeId,
        const TArray<FOGProjectResourceCost>& Costs,
        int64 StartWorldTick,
        int64 ResolveWorldTick,
        const FString& PayloadJson,
        FOGEntityId& OutProjectId,
        FString& OutError);

    /**
     * Lazy progress calculation. Nothing needs to tick while the project is
     * irrelevant/offscreen.
     */
    bool RefreshProject(
        const FOGEntityId& ProjectId,
        int64 CurrentWorldTick,
        FOGProjectRecord& OutProject,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
