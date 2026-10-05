#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGStrategyExpansionRecords.h"

using FOGLogisticsCapabilityValidator =
    TFunction<bool(
        const FOGEntityId& OwnerEntityId,
        const FOGContentId& TransportCapabilityId,
        const FOGEntityId& OriginLocationId,
        const FOGEntityId& OriginRealityId,
        const FOGEntityId& DestinationLocationId,
        const FOGEntityId& DestinationRealityId,
        FString& OutError)>;

/**
 * Data-driven civilization development and capability-grounded logistics.
 *
 * No universal modernization/era level exists. Route viability is validated
 * against the actual owned transport capability, including teleportation or
 * dimensional movement when content says it is valid.
 */
class OFFLINEGAME_API FOGCivilizationLogisticsService
{
public:
    explicit FOGCivilizationLogisticsService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool SetCivilizationState(
        const FOGCivilizationStateRecord& State,
        FString& OutError);

    bool SetCivilizationDimension(
        const FOGCivilizationDimensionRecord& Dimension,
        FString& OutError);

    bool SaveLogisticsRoute(
        const FOGLogisticsRouteRecord& Route,
        int64 WorldTick,
        const FOGLogisticsCapabilityValidator& CapabilityValidator,
        FString& OutError);

private:
    IOGWorldStore& Store;
};
