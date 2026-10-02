#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Math/OGLargeNumber.h"
#include "OGSkillReadiness.generated.h"

UENUM(BlueprintType)
enum class EOGReadinessRequirementKind : uint8
{
    ResourceAtLeast,
    StatePresent,
    StateAbsent,
    UsesThisBattleBelow,
    ReadyAtActionValue,
    HpAtLeastBps,
    HpAtMostBps,
    PreviousSkillIs
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSkillReadinessRequirement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGReadinessRequirementKind Kind = EOGReadinessRequirementKind::ResourceAtLeast;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ResourceKey = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ContentOperand;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 IntegerOperand = 0;
};

UENUM(BlueprintType)
enum class EOGSkillCostKind : uint8
{
    PersonalResource,
    HpPercentMax,
    FutureActionValue
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSkillCostDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGSkillCostKind Kind = EOGSkillCostKind::PersonalResource;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ResourceKey = NAME_None;

    /**
     * Resource amount, HP basis points, or future action-value amount depending
     * on Kind.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Amount = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSkillRuntimeState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SkillId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 UsesThisBattle = 0;

    /** Absolute action-value point when this skill becomes ready. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ReadyAtActionValue = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSkillUserRuntimeState
{
    GENERATED_BODY()

    /** Character-owned resources: mana, charge, ultimate charge, bespoke gauges. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FName, int64> PersonalResources;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGContentId> ActiveStates;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId PreviousSkillId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber CurrentHp;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber MaxHp;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 CurrentActionValue = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSkillReadinessResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bReady = false;

    /** Stable machine-readable reasons suitable for UI localization. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FName> FailureReasonCodes;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSkillActivationDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SkillId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGSkillReadinessRequirement> Requirements;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGSkillCostDefinition> Costs;

    /**
     * Optional action-value lockout after use. Zero means none.
     * This is how an actual cooldown can exist without making cooldown universal.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 CooldownActionValue = 0;
};

class OFFLINEGAME_API FOGSkillReadiness
{
public:
    static FOGSkillReadinessResult Evaluate(
        const FOGSkillActivationDefinition& Definition,
        const FOGSkillRuntimeState& SkillState,
        const FOGSkillUserRuntimeState& UserState);

    /**
     * Applies generic costs after a skill has been accepted.
     * Skill-specific costs may be handled by registered bespoke mechanics.
     */
    static bool ApplyCosts(
        const FOGSkillActivationDefinition& Definition,
        FOGSkillRuntimeState& SkillState,
        FOGSkillUserRuntimeState& UserState,
        int64& InOutFutureActionValue,
        FString& OutError);

private:
    static int32 ComputeHpBps(
        const FOGSkillUserRuntimeState& UserState);
};
