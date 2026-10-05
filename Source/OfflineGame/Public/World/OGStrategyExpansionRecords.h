#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "Math/OGLargeNumber.h"
#include "OGStrategyExpansionRecords.generated.h"

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDispatchObjectiveRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId DispatchId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ObjectiveId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bMandatory = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TargetEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDispatchConstraintRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId DispatchId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ConstraintId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT()
struct OFFLINEGAME_API FOGDispatchResolutionContext
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FOGDispatchObjectiveRecord> MandatoryObjectives;

    UPROPERTY()
    TArray<FOGDispatchConstraintRecord> HardConstraints;

    UPROPERTY()
    int32 RiskToleranceBps = 5000;

    UPROPERTY()
    FString AbortPolicyJson = TEXT("{}");

    UPROPERTY()
    TArray<FOGDispatchObjectiveRecord> SecondaryObjectives;
};

USTRUCT()
struct OFFLINEGAME_API FOGDispatchResolutionResult
{
    GENERATED_BODY()

    /** Lifecycle success/failure remains compatibility state; detail is OutcomeState. */
    UPROPERTY()
    bool bSucceeded = false;

    UPROPERTY()
    bool bMandatoryObjectivesEvaluated = false;

    UPROPERTY()
    bool bHardConstraintsEvaluated = false;

    UPROPERTY()
    bool bSurvivalAbortPolicyEvaluated = false;

    UPROPERTY()
    FName OutcomeState = NAME_None;

    UPROPERTY()
    FString ResultJson = TEXT("{}");

    UPROPERTY()
    bool bDelay = false;

    UPROPERTY()
    int64 DelayUntilWorldTick = 0;
};

using FOGDispatchResolver =
    TFunction<bool(
        const FOGDispatchResolutionContext& Context,
        FOGDispatchResolutionResult& OutResult,
        FString& OutError)>;

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWarFrontRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FrontId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId WarId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId LocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RealityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName State = FName(TEXT("active"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 StartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasEndWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 EndWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWarObjectiveRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId WarId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FrontId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ObjectiveId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TargetEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Status = FName(TEXT("active"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWarOrderRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OrderId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId WarId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FrontId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId IssuerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RecipientEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId IntentId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ConstraintsJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 IssuedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName OutcomeState = FName(TEXT("pending"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString OutcomeJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWarParticipantHistoryRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId WarId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FactionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SideIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 JoinedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasLeftWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 LeftWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Reason = NAME_None;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGArmyCapabilityRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ArmyId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId CapabilityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber Magnitude;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGProjectPhaseRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ProjectId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId PhaseId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Sequence = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Status = FName(TEXT("planned"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 StartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ResolveWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ProgressBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PayloadJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGProjectAssignmentRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ProjectId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId AssigneeEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId RoleId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCivilizationStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId CivilizationEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId GenreProfileId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCivilizationDimensionRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId CivilizationEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId DimensionId;

    /** Content-authored grade/state; not a universal era or modernization level. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Grade = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGLogisticsRouteRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RouteId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OriginLocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OriginRealityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId DestinationLocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId DestinationRealityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId TransportCapabilityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber Capacity;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 RiskBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Status = FName(TEXT("open"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};
