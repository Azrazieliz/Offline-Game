#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "World/OGRealityTimeRecords.h"

struct FOGCalendarElapsedQuery
{
    FOGContentId CalendarId;
    int64 StartLocalTick = 0;
    int64 EndLocalTick = 0;
    FName DurationKind = NAME_None;
    int32 DurationCount = 1;
    bool bStrictlyMoreThan = true;
};

using FOGCalendarElapsedResolver =
    TFunction<bool(
        const FOGCalendarElapsedQuery& Query,
        bool& bOutElapsed,
        FString& OutError)>;

/**
 * Deterministic canonical->local world-time projection.
 *
 * Calendar lengths and time ratios are content data. This service owns only
 * rational projection, hierarchy/cycle validation and resolver handoff.
 */
class OFFLINEGAME_API FOGWorldTimeService
{
public:
    explicit FOGWorldTimeService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool SaveTimeDomain(
        const FOGTimeDomainRecord& Domain,
        int64 CreatedWorldTick,
        FString& OutError);

    bool ProjectCanonicalTick(
        const FOGEntityId& TimeDomainId,
        int64 CanonicalWorldTick,
        FOGLocalTimeProjection& OutProjection,
        FString& OutError) const;

    bool HasStrictlyMoreThanCalendarDuration(
        const FOGEntityId& TimeDomainId,
        int64 StartCanonicalWorldTick,
        int64 EndCanonicalWorldTick,
        FName DurationKind,
        int32 DurationCount,
        const FOGCalendarElapsedResolver& Resolver,
        bool& bOutElapsed,
        FString& OutError) const;

private:
    bool BuildDomainChain(
        const FOGEntityId& TimeDomainId,
        TArray<FOGTimeDomainRecord>& OutRootToLeaf,
        FString& OutError) const;

    static bool ProjectOneDomain(
        const FOGTimeDomainRecord& Domain,
        int64 ParentTick,
        int64& OutLocalTick,
        FString& OutError);

    IOGWorldStore& Store;
};
