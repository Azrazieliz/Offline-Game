#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGStrategyExpansionRecords.h"
#include "World/OGTerritoryStateRecords.h"

/**
 * Generic lazy/event-driven Project service.
 *
 * Project timing/resource spending is independent from Domain Core semantics.
 * Optional complex phases are introduced by migration 0012 only where needed.
 */
class OFFLINEGAME_API FOGProjectService
{
public:
    explicit FOGProjectService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

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

    bool RefreshProject(
        const FOGEntityId& ProjectId,
        int64 CurrentWorldTick,
        FOGProjectRecord& OutProject,
        FString& OutError);

    bool SetProjectPhase(
        const FOGProjectPhaseRecord& Phase,
        FString& OutError);

    bool AssignProjectRole(
        const FOGProjectAssignmentRecord& Assignment,
        FString& OutError);

    /**
     * Refreshes authored phases lazily. Simple Projects may have no phase rows
     * and continue using RefreshProject directly.
     */
    bool RefreshProjectPhases(
        const FOGEntityId& ProjectId,
        int64 CurrentWorldTick,
        TArray<FOGProjectPhaseRecord>& OutPhases,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
