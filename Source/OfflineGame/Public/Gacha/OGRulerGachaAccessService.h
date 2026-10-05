#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGTerritoryStateRecords.h"

/**
 * Persistent first-unlock gate for the gameplay-earned gacha.
 *
 * This service derives qualification continuity from authoritative Territory
 * claims. It does not own calendar conversion.
 * bMoreThanOneInGameMonthElapsed must come from the authoritative world/time
 * layer and means strictly more than one local in-game month has elapsed since
 * the stored qualification start. Schema 0011 supplies that calendar mapping.
 */
class OFFLINEGAME_API FOGRulerGachaAccessService
{
public:
    explicit FOGRulerGachaAccessService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool CanUseGacha(
        const FOGEntityId& RulerId,
        int64 WorldTick,
        bool& bOutCanUse,
        FString& OutError) const;

    bool RefreshGachaQualification(
        const FOGEntityId& RulerId,
        int64 WorldTick,
        bool bMoreThanOneInGameMonthElapsed,
        FOGRulerGachaAccessRecord& OutAccess,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
