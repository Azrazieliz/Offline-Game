#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "OGWorldStateRecords.generated.h"

UENUM(BlueprintType)
enum class EOGLocationKnowledgeLevel : uint8
{
    Rumored = 1,
    Located = 2,
    Observed = 3,
    Explored = 4
};

/**
 * Physical/canonical location state.
 *
 * Discovery is intentionally not stored here: truth and knowledge are separate.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGLocationRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId LocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ParentLocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Kind = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TerritoryId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bPhysicallyAccessible = true;
};

/**
 * Authoritative physical presence used by both World Mode and Ruler Mode.
 *
 * Position is local to LocationId, avoiding one giant universe coordinate.
 * It is persisted at meaningful boundaries/checkpoints, not every rendered frame.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWorldPresenceRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId EntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId LocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FVector3d LocalPosition = FVector3d::ZeroVector;

    /** Ground / Sky / Underwater / Space / Underground / etc. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName MovementContext = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGKnowledgeFactRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName FactKey = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SubjectEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ValueJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 LearnedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    /** Subjective epistemic state; truth and belief remain separate. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName BeliefState = FName(TEXT("believed"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ConfidenceBps = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceEventId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasEvidenceWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 EvidenceWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId LanguageContextId;
};
