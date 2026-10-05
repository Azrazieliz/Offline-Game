#include "World/OGCivilizationLogisticsService.h"

bool FOGCivilizationLogisticsService::SetCivilizationState(
    const FOGCivilizationStateRecord& State,
    FString& OutError)
{
    return Store.UpsertCivilizationState(
        State,
        OutError);
}

bool FOGCivilizationLogisticsService::SetCivilizationDimension(
    const FOGCivilizationDimensionRecord& Dimension,
    FString& OutError)
{
    bool bFound = false;
    FOGCivilizationStateRecord Civilization;
    if (!Store.TryReadCivilizationState(
            Dimension.CivilizationEntityId,
            bFound,
            Civilization,
            OutError))
    {
        return false;
    }

    if (!bFound)
    {
        OutError =
            TEXT("Civilization dimension requires an existing civilization profile.");
        return false;
    }

    return Store.UpsertCivilizationDimension(
        Dimension,
        OutError);
}

bool FOGCivilizationLogisticsService::SaveLogisticsRoute(
    const FOGLogisticsRouteRecord& Route,
    int64 WorldTick,
    const FOGLogisticsCapabilityValidator& CapabilityValidator,
    FString& OutError)
{
    OutError.Reset();

    if (WorldTick < 0 ||
        !CapabilityValidator)
    {
        OutError =
            TEXT("Logistics route requires a valid world tick and capability validator.");
        return false;
    }

    if (!CapabilityValidator(
            Route.OwnerEntityId,
            Route.TransportCapabilityId,
            Route.OriginLocationId,
            Route.OriginRealityId,
            Route.DestinationLocationId,
            Route.DestinationRealityId,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError =
                TEXT("Transport capability cannot establish this Logistics route.");
        }
        return false;
    }

    return Store.UpsertLogisticsRoute(
        Route,
        WorldTick,
        OutError);
}
