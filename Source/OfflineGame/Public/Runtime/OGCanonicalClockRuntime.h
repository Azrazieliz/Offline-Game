#pragma once

#include "CoreMinimal.h"
#include "Persistence/OGWorldStore.h"
#include "Runtime/OGCanonicalClockState.h"
#include "World/OGWorldDirectorService.h"

enum class EOGCanonicalClockMode : uint8 { World, Ruler };

/** Versioned authored tuning. Source quanta are milliseconds, never calendar days. */
struct OFFLINEGAME_API FOGCanonicalClockPolicy
{
    FString PolicyId;
    int64 Version = 0;
    int64 RateDenominator = 0;
    int64 WorldRateNumerator = 0;
    int64 RulerRateNumerator = 0;
    int64 OfflineRateNumerator = 0;
    int64 TurnLocalRateDivisor = 0;
    int32 MaxDueActionsPerPump = 0;
    FOGContentId CalendarId;
    int64 LocalTicksPerDay = 0;
    int64 DaysPerMonth = 0;

    static bool LoadJsonFile(const FString& Path, FOGCanonicalClockPolicy& OutPolicy, FString& OutError);
    bool Validate(FString& OutError) const;
};

/** Content resolver owns idempotent domain consequences and persisted Reports.
 * The dispatcher retries active schedules after failure/restart. Resolver must
 * use ScheduleId as its idempotency key; notifications project committed Reports.
 */
using FOGCanonicalDueActionResolver = TFunction<bool(
    IOGWorldStore&, const FOGWorldDirectorScheduleRecord&, int64, bool, FString&, FString&)>;

/** Core-owned clock and bounded dispatcher. Uses the same canonical store and
 * existing Director; it does not implement content selection or time projection.
 * Clock entity and due registry travel with SQLite recovery/export/import.
 */
class OFFLINEGAME_API FOGCanonicalClockRuntime
{
public:
    explicit FOGCanonicalClockRuntime(IOGWorldStore& InStore) : Store(InStore), Director(InStore) {}
    bool LoadOrCreate(const FOGCanonicalClockPolicy& Policy, int64 NowUtcMilliseconds, FString& OutError);
    bool PumpOnline(double NowMonotonicSeconds, int64 NowUtcMilliseconds, FString& OutError);
    bool CatchupOffline(int64 NowUtcMilliseconds, FString& OutError);
    bool RecordBoundary(int64 NowUtcMilliseconds, FString& OutError);
    void ResetOnlineAnchor(double NowMonotonicSeconds);
    void SetMode(EOGCanonicalClockMode InMode) { Mode = InMode; }
    void SetPaused(bool bInPaused) { bPaused = bInPaused; }
    int64 GetCanonicalWorldTick() const { return bLoaded ? State.CurrentWorldTick : -1; }
    const FOGCanonicalClockPolicy& GetPolicy() const { return Policy; }
    static FOGEntityId ClockEntityId();
    void RegisterResolver(FName Key, FOGCanonicalDueActionResolver Resolver);
    bool RegisterDueAction(const FOGEntityId& ScheduleId, FName ResolverKey,
        const FOGEntityId& ScopeId, int32 RequestedLossBps, int32 MaximumLossBps,
        bool bCausallyLocked, bool bProtectedCollapse, FString& OutError);
    bool SetLocalTurnBattle(const FOGEntityId& DomainId, bool bActive, FString& OutError);

private:
    struct FDueAction
    {
        FOGEntityId ScheduleId;
        FName ResolverKey;
        FOGEntityId ScopeId;
        int32 RequestedLossBps = 0;
        int32 MaximumLossBps = 0;
        bool bCausallyLocked = false;
        bool bProtectedCollapse = false;
        bool bOfflinePending = false;
    };
    bool Save(const FOGCanonicalClockState& Candidate, int64 UtcAnchor, FString& OutError);
    bool AdvanceSource(int64 Milliseconds, int64 Numerator, int64 UtcAnchor, bool bOffline, FString& OutError);
    bool DrainDueActions(bool bOffline, FString& OutError);
    IOGWorldStore& Store;
    FOGWorldDirectorService Director;
    FOGCanonicalClockPolicy Policy;
    FOGCanonicalClockState State;
    TArray<FDueAction> DueActions;
    TMap<FName, FOGCanonicalDueActionResolver> Resolvers;
    int64 LastUtcMilliseconds = 0;
    uint64 CommitRevision = 0;
    double LastMonotonicSeconds = 0;
    double OnlineFractionalMilliseconds = 0;
    EOGCanonicalClockMode Mode = EOGCanonicalClockMode::World;
    bool bHasOnlineAnchor = false;
    bool bLoaded = false;
    bool bPaused = false;
};
