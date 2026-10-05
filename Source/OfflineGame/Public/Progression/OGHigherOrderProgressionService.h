#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Progression/OGProgressionRecords.h"

/**
 * Explicit persistence for irreversible Transcendence and higher-order personal
 * reality expressions. Qualification/proof generation is content/history driven
 * and supplied already resolved; no universal hidden quest checklist is encoded.
 */
class OFFLINEGAME_API FOGHigherOrderProgressionService
{
public:
    explicit FOGHigherOrderProgressionService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool CommitValidatedTranscendence(
        const FOGEntityTranscendenceStateRecord& State,
        FString& OutError);

    bool UnlockOrEvolveWorldFantasm(
        const FOGManifestationWorldFantasmStateRecord& State,
        FString& OutError);

    bool UnlockOrEvolveProtagonistWorldManifestation(
        const FOGProtagonistWorldManifestationStateRecord& State,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
