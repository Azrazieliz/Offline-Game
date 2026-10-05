#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGTerritoryStateRecords.h"

/**
 * Projects current/historical universal sovereignty state from real Territory
 * claims. Ruler and Overlord are the only universal sovereignty titles.
 *
 * Overlord qualification depends on systems reconciled later (personal Rank /
 * Authority plus actual high-order sovereignty), so that resolved boolean is an
 * input rather than an invented local score.
 */
class OFFLINEGAME_API FOGSovereigntyService
{
public:
    explicit FOGSovereigntyService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool RefreshSovereigntyState(
        const FOGEntityId& RulerId,
        int64 WorldTick,
        bool bMeetsOverlordRequirements,
        const FString& ScopeStateJson,
        FOGRulerSovereigntyStateRecord& OutState,
        FString& OutError);

private:
    static bool IsEffectiveState(FName ControlState);

    IOGWorldStore& Store;
};
