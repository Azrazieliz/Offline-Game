#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGWorldStateRecords.h"

/**
 * Shared game-core service used by both Ruler Mode and World Mode.
 *
 * Neither mode owns a private canonical copy of location/presence/discovery.
 */
class OFFLINEGAME_API FOGSharedWorldStateService
{
public:
    explicit FOGSharedWorldStateService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool RecordLocationDiscovery(
        const FOGEntityId& KnowledgeOwnerId,
        const FOGEntityId& LocationId,
        EOGLocationKnowledgeLevel Level,
        int64 WorldTick,
        FString& OutError);

    bool TryReadLocationKnowledge(
        const FOGEntityId& KnowledgeOwnerId,
        const FOGEntityId& LocationId,
        bool& bOutKnown,
        EOGLocationKnowledgeLevel& OutLevel,
        FString& OutError) const;

    bool UpdatePhysicalPresence(
        const FOGWorldPresenceRecord& Presence,
        FString& OutError);

    bool TryReadPhysicalPresence(
        const FOGEntityId& EntityId,
        bool& bOutFound,
        FOGWorldPresenceRecord& OutPresence,
        FString& OutError) const;

    static FName LocationKnowledgeFactKey();

private:
    IOGWorldStore& Store;
};
