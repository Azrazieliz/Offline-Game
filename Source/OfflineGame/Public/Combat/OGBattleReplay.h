#pragma once

#include "CoreMinimal.h"
#include "Combat/OGCombatMath.h"
#include "Combat/OGTurnBattle.h"
#include "OGBattleReplay.generated.h"

/**
 * One deterministic player/AI combat decision for the replay proof.
 *
 * The command stores intent and authored scalar input, not random outcomes.
 * RNG outcomes are regenerated from the replay seed.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGReplayActionCommand
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceUnitId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TargetUnitId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SkillId;

    /** Ordinary ATK multiplier; 10,000 = x1.0. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SkillMultiplierBps = 10000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGBaseDamageType DamageType = EOGBaseDamageType::Physical;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bCanCrit = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bCanBeBlocked = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAllowHitOverflowReplication = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ActionDelay = 1000;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGReplayResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bSucceeded = false;

    UPROPERTY(BlueprintReadOnly)
    uint64 Seed = 0;

    UPROPERTY(BlueprintReadOnly)
    uint64 RngDrawCount = 0;

    UPROPERTY(BlueprintReadOnly)
    FString DeterministicFingerprint;

    UPROPERTY(BlueprintReadOnly)
    FString Error;
};

/**
 * Minimal replay harness proving deterministic combat execution.
 *
 * It is intentionally not a save format. Later diagnostics can serialize the
 * same seed + initial state + command stream.
 */
class OFFLINEGAME_API FOGBattleReplay
{
public:
    static FOGReplayResult Run(
        const FOGTurnBattleState& InitialState,
        uint64 Seed,
        const TArray<FOGReplayActionCommand>& Commands);

    static FOGReplayResult Run(
        const FOGTurnBattleState& InitialState,
        uint64 Seed,
        const TArray<FOGReplayActionCommand>& Commands,
        FOGIdentityExclusivityContext IdentityContext,
        FOGRankSuppressionResolver RankResolver);

private:
    static const FOGCombatUnitState* FindUnit(
        const FOGTurnBattleState& State,
        const FOGEntityId& UnitId);

    static FString BuildFingerprint(
        const FOGTurnBattle& Battle,
        uint64 Seed,
        uint64 DrawCount);
};
