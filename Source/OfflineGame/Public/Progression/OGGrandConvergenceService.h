#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Progression/OGProgressionRecords.h"

/**
 * Identity-level finalization of multiple divergent Manifestations.
 *
 * The result Manifestation is supplied after content-specific synthesis. This
 * service validates ownership/Identity/max-reinforcement, persists lineage, and
 * retires sources from deployment without deleting any source progression.
 */
class OFFLINEGAME_API FOGGrandConvergenceService
{
public:
    explicit FOGGrandConvergenceService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool PerformConvergence(
        FOGCharacterManifestationRecord ResultManifestation,
        const TArray<FOGEntityId>& SourceManifestationIds,
        const TArray<FOGContentId>& SourceLineageIds,
        const FOGContentId& RuleId,
        int64 WorldTick,
        const FString& StateJson,
        FOGEntityId& OutConvergenceId,
        FString& OutError);

    bool PerformConvergence(
        FOGCharacterManifestationRecord ResultManifestation,
        const TArray<FOGEntityId>& SourceManifestationIds,
        const TArray<FOGContentId>& SourceLineageIds,
        const FOGContentId& RuleId,
        int64 WorldTick,
        const FString& StateJson,
        bool bExplicitlyConfirmProtectedSources,
        FOGEntityId& OutConvergenceId,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
