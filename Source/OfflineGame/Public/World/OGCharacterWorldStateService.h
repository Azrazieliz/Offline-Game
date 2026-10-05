#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGItemKnowledgeCharacterRecords.h"
#include "World/OGWorldStateRecords.h"

class OFFLINEGAME_API FOGCharacterWorldStateService
{
public:
    explicit FOGCharacterWorldStateService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool RecordKnowledge(
        const FOGKnowledgeFactRecord& Fact,
        FString& OutError);

    bool RecordLanguage(
        const FOGEntityLanguageRecord& Language,
        FString& OutError);

    bool RecordSemanticMemory(
        const FOGSemanticMemoryRecord& Memory,
        int64 WorldTick,
        FString& OutError);

    bool PromoteNpc(
        const FOGEntityId& ExistingEntityId,
        const FOGContentId& SimulationTierId,
        int64 WorldTick,
        const FOGEntityId& ReasonEventId,
        const FString& PresentationPackageStateJson,
        FString& OutError);

    bool SetAdultRuntimeContext(
        const FOGCharacterAdultRuntimeStateRecord& State,
        FString& OutError);

    bool CreateHeroicRecord(
        const FOGEntityId& SourceWorldEntityId,
        const FOGContentId& IdentityId,
        const FOGEntityId& DeathEventId,
        const FOGContentId& PatternId,
        int64 WorldTick,
        FOGEntityId& OutRecordId,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
