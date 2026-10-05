#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Progression/OGProgressionRecords.h"

/**
 * Data-driven Existence/Power Rank state.
 *
 * Rank ordering/coefficients live in validated content/tuning data. This service
 * deliberately does not compare Rank content IDs lexically or encode the named
 * ladder as a C++ enum.
 */
class OFFLINEGAME_API FOGRankService
{
public:
    explicit FOGRankService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool SetRankState(
        const FOGEntityRankStateRecord& State,
        FString& OutError);

    bool ResolveEffectiveRank(
        const FOGEntityId& EntityId,
        bool& bOutFound,
        FOGResolvedRankProjection& OutProjection,
        FString& OutError) const;

private:
    IOGWorldStore& Store;
};
