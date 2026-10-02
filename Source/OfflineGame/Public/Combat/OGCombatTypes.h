#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "Math/OGLargeNumber.h"
#include "Skills/OGResolvedSkillSet.h"
#include "OGCombatTypes.generated.h"

/** Percentages use basis points: 10,000 = 100%. */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCombatStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber MaxHp;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber Attack;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber Defense;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Speed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CritRateBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CritDamageBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CritResistanceBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 HitBps = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DodgeBps = 0;
};

UENUM(BlueprintType)
enum class EOGCombatPresence : uint8
{
    Active,
    Reserve,
    Defeated,
    Removed
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCombatUnitState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId UnitEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 TeamIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGCombatPresence Presence = EOGCombatPresence::Reserve;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGCombatStats Stats;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber CurrentHp;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGResolvedSkillSet SkillSet;

    /** Continuous action-value/timeline position. Lower acts sooner. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 NextActionValue = 0;

    bool IsAlive() const
    {
        return FOGLargeNumber::Compare(CurrentHp, FOGLargeNumber()) > 0;
    }

    bool CanAct() const
    {
        return Presence == EOGCombatPresence::Active && IsAlive();
    }
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGResolvedCombatAction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ActionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceUnitId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGEntityId> TargetUnitIds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SkillId;

    /**
     * Timeline delay already resolved by the timing/stat layer.
     * The executor deliberately does not hard-code a Speed formula.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ActionDelay = 0;

    /** Ultimates/counters/etc. may interrupt the ordinary queue. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 InterruptPriority = 0;

    /** Pre-resolved authoritative action payload for the first executor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ResolutionJson = TEXT("{}");
};
