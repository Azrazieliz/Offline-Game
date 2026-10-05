#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGTerritoryStateRecords.h"

/**
 * Domain Core / Territory-heart authority.
 *
 * Broken-versus-captured is a hard invariant. Physical Territory devastation
 * is deliberately not stored here: an intact protected Core keeps the Domain
 * metaphysically functional while ordinary reconstruction happens elsewhere.
 *
 * Concept synthesis is bounded/data-driven. The caller supplies a validated
 * synthesis rule and any resulting Concepts; this service persists the
 * asymmetric resolution, seed, instability and lineage without inventing
 * executable abilities from arbitrary names.
 */
class OFFLINEGAME_API FOGDomainCoreService
{
public:
    explicit FOGDomainCoreService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool ActivateAwakenedCoreAsHeart(
        const FOGEntityId& CoreId,
        int64 WorldTick,
        FString& OutError);

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

    bool FuseCores(
        const FOGDomainCoreFusionRequest& Request,
        int64 WorldTick,
        FOGDomainCoreFusionRecord& OutFusion,
        FString& OutError);

    /**
     * Re-evaluates the metaphysical heart from persisted Core state. The caller
     * supplies only whether the authored/high-order ruin threshold has been
     * reached; no hard-coded ruin duration is invented here.
     */
    bool RefreshDomainHeartConsequences(
        const FOGEntityId& TerritoryId,
        int64 WorldTick,
        bool bRuinThresholdReached,
        FOGTerritoryDomainStateRecord& OutState,
        FString& OutError);

    /**
     * Links a separately validated exceptional recovery Project. Ordinary
     * construction/repair Projects must not call this automatically.
     */
    bool BeginHeartReconstitution(
        const FOGEntityId& TerritoryId,
        const FOGEntityId& ReconstitutionProjectId,
        int64 WorldTick,
        FString& OutError);

    bool CompleteHeartReconstitution(
        const FOGEntityId& TerritoryId,
        const FOGEntityId& NewCoreId,
        int64 WorldTick,
        FString& OutError);

private:
    bool EnsureConceptProjection(
        const FOGDomainCoreRecord& Core,
        TArray<FOGDomainCoreConceptRecord>& OutConcepts,
        FString& OutError);

    bool MarkHeartLostIfActive(
        const FOGDomainCoreRecord& Core,
        int64 WorldTick,
        FString& OutError);

    static FName HeartStateForIntactCore(
        const FOGDomainCoreRecord& Core);

    IOGWorldStore& Store;
};
