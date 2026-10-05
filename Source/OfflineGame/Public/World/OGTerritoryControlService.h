#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGTerritoryStateRecords.h"

/**
 * Authoritative Territory-claim control operations.
 *
 * Physical ownership/control is resolved from normalized claims. Legacy
 * Territory ruler/control fields are presentation caches only after schema 0008.
 * The five-day deadline is supplied in canonical world ticks by the authoritative
 * calendar/time layer; this service never invents a day-to-tick ratio.
 */
class OFFLINEGAME_API FOGTerritoryControlService
{
public:
    explicit FOGTerritoryControlService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool EvaluateEffectiveControl(
        const FOGEntityId& TerritoryId,
        int64 WorldTick,
        FOGTerritoryEffectiveControlResult& OutResult,
        FString& OutError) const;

    bool BeginDisplacement(
        const FOGEntityId& ClaimId,
        int64 WorldTick,
        int64 FiveDayReclaimDeadlineWorldTick,
        FString& OutError);

    bool ReclaimTerritory(
        const FOGEntityId& ClaimId,
        int64 WorldTick,
        FString& OutError);

    bool ExpireReclamationIfDue(
        const FOGEntityId& ClaimId,
        int64 WorldTick,
        bool& bOutExpired,
        FString& OutError);

    bool ListActiveClaimsForLocation(
        const FOGEntityId& LocationId,
        TArray<FOGTerritoryClaimRecord>& OutClaims,
        FString& OutError) const;

    /**
     * Deterministically chooses one currently effective Territory for World Mode
     * anchoring. Main Territory wins, then oldest claim, then stable ID.
     * No result is not an error: permanent gacha access can outlive all Territory.
     */
    bool FindPreferredEffectiveTerritoryForRuler(
        const FOGEntityId& RulerId,
        int64 WorldTick,
        FOGEntityId& OutTerritoryId,
        FString& OutError) const;

private:
    static bool IsEffectiveState(FName ControlState);

    IOGWorldStore& Store;
};
