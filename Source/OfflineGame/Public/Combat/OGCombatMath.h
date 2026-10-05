#pragma once

#include "CoreMinimal.h"
#include "Combat/OGCombatTypes.h"
#include "Random/OGDeterministicRng.h"
#include "OGCombatMath.generated.h"

UENUM(BlueprintType)
enum class EOGBaseDamageType : uint8
{
    Physical,
    True
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCritResolution
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 EffectiveCritRateBps = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 CritChanceBps = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 OverflowCritRateBps = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 OverflowCritDamageBonusBps = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 EffectiveCritDamageBonusBps = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bCritical = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGHitResolution
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 EffectiveHitBps = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 GuaranteedHitInstances = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 OverflowRemainderBps = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 ResolvedHitInstances = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGBlockResolution
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 EffectiveBlockRateBps = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 BlockReductionBps = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bBlocked = false;
};

/**
 * Input to baseline damage math after a skill has resolved its own scaling.
 *
 * BaseDamage is normally ATK x skill multiplier, but may come from HP, DEF,
 * target stats, missing HP, or another explicit skill rule.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDamageRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber BaseDamage;

    /**
     * Reference used by progressive DEF mitigation.
     * Normally the attacker's relevant scale stat; caller chooses it explicitly.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber DefenseReference;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGBaseDamageType DamageType = EOGBaseDamageType::Physical;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bCanCrit = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bCanBeBlocked = true;

    /**
     * Whether Hit overflow may create replicated hit instances.
     * Set false for a fiction/mechanic-defined indivisible strike.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAllowHitOverflowReplication = true;

    /**
     * Resolved channel-specific Rank Suppression multiplier supplied by the
     * shared Rank hook. 10,000 = no suppression.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 RankSuppressionMultiplierBps = 10000;

    /** Additional authored multiplier categories; 10,000 = x1.0 each. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<int32> AdditionalMultiplierBps;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDamageResolution
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGHitResolution Hit;

    UPROPERTY(BlueprintReadOnly)
    FOGCritResolution Crit;

    UPROPERTY(BlueprintReadOnly)
    FOGBlockResolution Block;

    UPROPERTY(BlueprintReadOnly)
    int32 DefenseMultiplierBps = 10000;

    UPROPERTY(BlueprintReadOnly)
    int32 RankSuppressionMultiplierBps = 10000;

    UPROPERTY(BlueprintReadOnly)
    FOGLargeNumber DamagePerHit;

    UPROPERTY(BlueprintReadOnly)
    FOGLargeNumber TotalDamage;
};

/**
 * Baseline deterministic combat math.
 *
 * Skill-specific rules may bypass/replace pieces through the Authority/rule
 * layer, but ordinary attacks share this implementation.
 */
class OFFLINEGAME_API FOGCombatMath
{
public:
    static FOGCritResolution ResolveCrit(
        const FOGCombatStats& Attacker,
        const FOGCombatStats& Defender,
        FOGDeterministicRng& Rng);

    static FOGHitResolution ResolveHit(
        const FOGCombatStats& Attacker,
        const FOGCombatStats& Defender,
        bool bAllowOverflowReplication,
        FOGDeterministicRng& Rng);

    static FOGBlockResolution ResolveBlock(
        const FOGCombatStats& Defender,
        bool bCanBeBlocked,
        FOGDeterministicRng& Rng);

    /**
     * Progressive DEF factor = reference / (reference + effective DEF).
     * DEF penetration is percentage-based and can exceed 100% where allowed.
     * Negative-defense pressure is represented as bonus multiplier.
     */
    static int32 ResolveDefenseMultiplierBps(
        const FOGLargeNumber& DefenseReference,
        const FOGLargeNumber& DefenderDefense,
        int32 DefensePenetrationBps);

    static FOGDamageResolution ResolveDamage(
        const FOGDamageRequest& Request,
        const FOGCombatStats& Attacker,
        const FOGCombatStats& Defender,
        FOGDeterministicRng& Rng);

private:
    static bool RollBasisPoints(
        int32 ChanceBps,
        FOGDeterministicRng& Rng);

    static int32 RatioBps(
        const FOGLargeNumber& Numerator,
        const FOGLargeNumber& OtherPositiveTerm);
};
