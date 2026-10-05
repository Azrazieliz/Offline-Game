#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "OGProgressionRecords.generated.h"

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEntityRankStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId EntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId AttainedRankId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 AttainedLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId EffectiveRankId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 EffectiveLevel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId PeakRankId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 PeakLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGResolvedRankProjection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId RankId;

    UPROPERTY(BlueprintReadOnly)
    int32 Level = 1;

    UPROPERTY(BlueprintReadOnly)
    bool bUsingEffectiveOverride = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGFactorInstanceRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FactorInstanceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId FactorId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 AcquiredWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 PurityBps = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaturityBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CompletenessBps = 10000;

    /** Expression emphasis only; it never disables other owned Factors. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ExpressionWeightBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGFactorLineageRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ChildFactorInstanceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ParentFactorInstanceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName RelationKind = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Ordinal = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEntityClassRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ClassId;

    /** normal / crown or content-extensible mastery tier. Grand is a seat. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName AttainedTier = FName(TEXT("normal"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName CurrentExpressionState = FName(TEXT("expressible"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 RecognizedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGrandClassSeatRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ClassId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId BearerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 AppointedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName SeatState = FName(TEXT("active"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString MandateStateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEntitySkillRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SkillId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 LearnedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName CurrentState = FName(TEXT("learned"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString DevelopmentStateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSkillProvenanceRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SkillId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName SourceKind = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString SourceContentOrEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 SourceWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGManifestationRouteNodeRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId RouteId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId NodeId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName State = FName(TEXT("available"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasEnteredWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 EnteredWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasCompletedWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 CompletedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGManifestationFormRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId FormId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName State = FName(TEXT("unlocked"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UnlockedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGManifestationReinforcementRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ReinforcementState = FName(TEXT("developing"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bMaxReinforced = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEntityTranscendenceStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId EntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId GradeId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 BreakthroughWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString QualificationSnapshotJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ProofProvenanceJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGManifestationWorldFantasmStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId GradeId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UnlockedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ExpressionProfileId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString EvolutionStateJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGProtagonistWorldManifestationStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UnlockedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ExpressionProfileId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ProvenanceStateJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString EvolutionStateJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCharacterConvergenceRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ConvergenceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ResultManifestationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId RuleId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 WorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCharacterConvergenceSourceRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ConvergenceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceManifestationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId LineageId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Ordinal = 0;
};
