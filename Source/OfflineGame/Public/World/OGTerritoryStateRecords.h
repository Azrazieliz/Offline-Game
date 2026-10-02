#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "Math/OGLargeNumber.h"
#include "OGTerritoryStateRecords.generated.h"

UENUM(BlueprintType)
enum class EOGDomainCoreLifecycle : uint8
{
    Dormant,
    Awakened,
    Broken,
    Absorbed
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTerritoryRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TerritoryId;

    /** Invalid means currently neutral/unclaimed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RulerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RootLocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bMainTerritory = false;

    /** Aggregate only. No individual demographic simulation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Population = 0;

    /** Controlled / Disputed / Neutral / etc.; data-level state, not politics. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ControlState = TEXT("controlled");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDomainCoreAspect
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId AspectId;

    /** Optional authored progression grade for this Aspect. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Grade = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDomainCoreRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId CoreId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TerritoryId;

    /** Invalid when uncontrolled/neutral/broken as appropriate. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ControllerRulerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGDomainCoreLifecycle Lifecycle = EOGDomainCoreLifecycle::Dormant;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber CurrentDurability;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber MaxDurability;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGDomainCoreAspect> Aspects;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGResourceBalance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ResourceId;

    /** Resources are ordinary aggregate counters unless proven otherwise. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Amount = 0;
};

UENUM(BlueprintType)
enum class EOGProjectStatus : uint8
{
    Planned,
    Active,
    Completed,
    Failed,
    Cancelled
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGProjectRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ProjectId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId LocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ProjectTypeId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGProjectStatus Status = EOGProjectStatus::Planned;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 StartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ResolveWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ProgressBps = 0;

    /** Sparse project-specific state; searchable fields remain normalized. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PayloadJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGProjectResourceCost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ResourceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Amount = 0;
};
