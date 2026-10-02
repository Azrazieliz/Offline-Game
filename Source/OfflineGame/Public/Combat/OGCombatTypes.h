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

    /**
     * Bonus above ordinary non-critical damage.
     * 5,000 = +50% Crit Damage, i.e. a x1.5 critical before other modifiers.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CritDamageBonusBps = 5000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CritRateResistanceBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CritDamageResistanceBps = 0;

    /** 10,000 Hit against 0 Dodge = one guaranteed normal hit. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 HitBps = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DodgeBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 BlockRateBps = 0;

    /** Damage removed when a block succeeds. 3,000 = 30% reduction. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 BlockReductionBps = 0;

    /** Percentage-point DEF penetration. Values may exceed 100% where allowed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DefensePenetrationBps = 0;

    /** Explicit multiplicative category; 10,000 = x1.0. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DamageDealtMultiplierBps = 10000;

    /** Explicit multiplicative category; 10,000 = x1.0. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 DamageTakenMultiplierBps = 10000;

    /** Applies specifically to True Damage. */
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

    /** Continuous action-value/timeline position. Lower acts sooner. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 NextActionValue = 0;

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
