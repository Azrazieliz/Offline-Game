#pragma once

#include "CoreMinimal.h"
#include "Core/OGEntityId.h"
#include "Math/OGLargeNumber.h"
#include "OGDispatchFactionWarRecords.generated.h"

UENUM(BlueprintType)
enum class EOGDispatchType : uint8
{
    Exploration,
    Gathering,
    Reconnaissance,
    Espionage,
    Support,
    Other
};

UENUM(BlueprintType)
enum class EOGDispatchStatus : uint8
{
    Planned,
    Active,
    Succeeded,
    Failed,
    Cancelled
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDispatchRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId DispatchId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    /** Optional physical/faction/entity target. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TargetEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGDispatchType Type = EOGDispatchType::Exploration;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGDispatchStatus Status = EOGDispatchStatus::Planned;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGEntityId> ParticipantEntityIds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 StartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ResolveWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 RiskBps = 0;

    /** Mission-authorized risk appetite; 10,000 = maximum tolerance. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 RiskToleranceBps = 5000;

    /** Explicit return/abort conditions supplied by the issuing player/system. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString AbortPolicyJson = TEXT("{}");

    /** Nuanced resolved/delayed/aborted/partial outcome; content-extensible. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName OutcomeState = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasDelayUntilWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 DelayUntilWorldTick = 0;

    /** Recorded deterministic provenance for the eventual resolver. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ResolutionSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ResultJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGFactionRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FactionId;

    /** Optional Ruler/leader; invalid is allowed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId LeaderRulerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Kind = FName(TEXT("faction"));

    /** Aggregate only; no demographic agent simulation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Population = 0;
};

UENUM(BlueprintType)
enum class EOGFactionLinkType : uint8
{
    Alliance,
    Support,
    Subordination
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGFactionLinkRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceFactionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TargetFactionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGFactionLinkType Type = EOGFactionLinkType::Alliance;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bActive = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGArmyRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ArmyId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FactionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId LocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Headcount = 0;

    /**
     * Cached summary/debug projection only. Authoritative strategic resolution
     * consumes normalized capability vectors plus named-character intervention.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber EffectivePower;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGEntityId> CommanderEntityIds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName State = FName(TEXT("ready"));
};

UENUM(BlueprintType)
enum class EOGWarStatus : uint8
{
    Active,
    Resolved,
    Withdrawn,
    Collapsed
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWarParticipant
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FactionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SideIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bPrimary = true;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWarRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId WarId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGWarStatus Status = EOGWarStatus::Active;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ObjectiveType = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ObjectiveTargetEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 StartWorldTick = 0;

    /** Zero while active. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 EndWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGWarParticipant> Participants;

    /** Concise outcome state, not a treaty/legal document. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ResolutionJson = TEXT("{}");
};
