#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGTerritoryStateRecords.h"
#include "World/OGWorldTimeService.h"

/**
 * Persistent first-unlock gate for the gameplay-earned gacha.
 *
 * Qualification continuity is derived from authoritative Territory claims.
 * Elapsed "one in-game month" is resolved through the Territory/World's
 * authored Time Domain + Calendar. This service never invents a month length.
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
        const FOGEntityId& QualificationTimeDomainId,
        const FOGCalendarElapsedResolver& CalendarResolver,
        FOGRulerGachaAccessRecord& OutAccess,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
