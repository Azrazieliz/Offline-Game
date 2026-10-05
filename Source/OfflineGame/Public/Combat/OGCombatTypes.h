#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "Math/OGLargeNumber.h"
#include "Progression/OGProgressionRecords.h"
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
    int32 CritDamageBonusBps = 5000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CritRateResistanceBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CritDamageResistanceBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 HitBps = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DodgeBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 BlockRateBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 BlockReductionBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DefensePenetrationBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DamageDealtMultiplierBps = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DamageTakenMultiplierBps = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 TrueDamageResistanceBps = 0;
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

    /**
     * Resolved progression projection supplied by the authoritative progression
     * layer. Combat never owns or mutates attained/effective Rank truth.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGResolvedRankProjection RankProjection;

    /** Continuous action-value/timeline position. Lower acts sooner. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 NextActionValue = 0;

    /**
     * Current occupied battlefield lane, 0..5 while active.
     * This is deliberately separate from the unit's original succession lane:
     * reserves may dynamically fill an exhausted lane during battle.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 OccupiedLaneIndex = INDEX_NONE;

    /**
     * Normal entry delay resolved by the timing layer (typically from SPD).
     * Successors use this when entering after a defeat so entry is not a free turn.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 DefaultActionDelay = 1000;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ActionDelay = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 InterruptPriority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ResolutionJson = TEXT("{}");
};
