#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "OGRealityTimeRecords.generated.h"

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGRealityNodeRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RealityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ParentRealityId;

    /** world / dimension / subrealm / universe / interstice / etc. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Kind = NAME_None;

    /** Data-driven World Rank. Empty when unknown/not applicable. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId WorldRankId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TimeDomainId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId LawProfileId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTimeDomainRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TimeDomainId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ParentTimeDomainId;

    /** Local ticks per parent tick = numerator / denominator. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 RateNumerator = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 RateDenominator = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ParentEpochTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 LocalEpochTick = 0;

    /** Calendar semantics are content-authored, not hard-coded Earth time. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId CalendarId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGLocalTimeProjection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId TimeDomainId;

    UPROPERTY(BlueprintReadOnly)
    int64 CanonicalWorldTick = 0;

    UPROPERTY(BlueprintReadOnly)
    int64 LocalTick = 0;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId CalendarId;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGJunctionRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId JunctionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FromRealityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ToRealityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName State = FName(TEXT("closed"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 StabilityBps = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasOpenedWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 OpenedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasClosedWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ClosedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString RequirementsJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWorldDirectorScheduleRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ScheduleId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ContentId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId TemplateId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Status = FName(TEXT("scheduled"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasEligibleSinceWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 EligibleSinceWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasScheduledStartWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ScheduledStartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasLatestStartWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 LatestStartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ResolutionSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString DecisionProvenanceJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGContentUnlockStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ContentId;

    /** sealed / eligible / released / retired / content-extensible. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName State = FName(TEXT("sealed"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasEligibleWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 EligibleWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasReleasedWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ReleasedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGOfflineSimulationStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ScopeEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 LastActiveWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 LastCatchupWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString GovernorStateJson = TEXT("{}");
};
