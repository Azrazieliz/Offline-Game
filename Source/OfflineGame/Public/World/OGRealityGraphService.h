#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGRealityTimeRecords.h"

/**
 * Persistent reality graph and Junction lifecycle.
 *
 * World Rank identities/order and Junction requirements remain content data.
 * This service preserves causal graph integrity without inventing progression
 * gates or a fixed final cosmological hierarchy.
 */
class OFFLINEGAME_API FOGRealityGraphService
{
public:
    explicit FOGRealityGraphService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool SaveRealityNode(
        const FOGRealityNodeRecord& Reality,
        int64 CreatedWorldTick,
        FString& OutError);

    bool SaveJunction(
        const FOGJunctionRecord& Junction,
        int64 CreatedWorldTick,
        FString& OutError);

    bool SetWorldRank(
        const FOGEntityId& RealityId,
        const FOGContentId& NewWorldRankId,
        int64 WorldTick,
        const FString& ProvenanceJson,
        FString& OutError);

    bool OpenJunction(
        const FOGEntityId& JunctionId,
        int64 WorldTick,
        bool bRequirementsSatisfied,
        FString& OutError);

    bool CloseJunction(
        const FOGEntityId& JunctionId,
        int64 WorldTick,
        FName Reason,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
