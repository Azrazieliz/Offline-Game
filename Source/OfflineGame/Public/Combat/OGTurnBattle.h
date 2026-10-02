#pragma once

#include "CoreMinimal.h"
#include "Combat/OGCombatLog.h"
#include "Combat/OGCombatTypes.h"
#include "OGTurnBattle.generated.h"

UENUM(BlueprintType)
enum class EOGTurnBattleStatus : uint8
{
    NotStarted,
    Running,
    Completed
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTurnTeamState
{
    GENERATED_BODY()

    static constexpr int32 MaxRosterSize = 18;
    static constexpr int32 MaxActiveSize = 6;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 TeamIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGEntityId> RosterUnitIds;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTurnBattleState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId BattleId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGTurnBattleStatus Status = EOGTurnBattleStatus::NotStarted;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGTurnTeamState> Teams;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGCombatUnitState> Units;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 CurrentActionValue = 0;
};

/**
 * Minimal deterministic turn executor.
 *
 * It owns ordering/state transitions, not damage formulas. Resolution data is
 * supplied by the rule/combat-math layer so the executor can remain stable while
 * formulas evolve.
 */
class OFFLINEGAME_API FOGTurnBattle
{
public:
    bool Initialize(
        const FOGTurnBattleState& InitialState,
        FString& OutError);

    const FOGTurnBattleState& GetState() const
    {
        return State;
    }

    const FOGCombatLog& GetLog() const
    {
        return Log;
    }

    /** Returns INDEX_NONE when no active living unit can act. */
    int32 SelectNextActingUnitIndex() const;

    /**
     * Resolves ordering metadata for an already-resolved action.
     * InterruptPriority > 0 allows an action to resolve before ordinary action.
     */
    bool ApplyResolvedAction(
        const FOGResolvedCombatAction& Action,
        FString& OutError);

    bool ApplyDamage(
        const FOGEntityId& SourceUnitId,
        const FOGEntityId& TargetUnitId,
        const FOGLargeNumber& Amount,
        const FOGContentId& SkillId,
        FString& OutError);

    bool ApplyHealing(
        const FOGEntityId& SourceUnitId,
        const FOGEntityId& TargetUnitId,
        const FOGLargeNumber& Amount,
        const FOGContentId& SkillId,
        FString& OutError);

    bool ChangePresence(
        const FOGEntityId& UnitId,
        EOGCombatPresence NewPresence,
        FString& OutError);

private:
    FOGCombatUnitState* FindMutableUnit(const FOGEntityId& UnitId);
    const FOGCombatUnitState* FindUnit(const FOGEntityId& UnitId) const;
    void EvaluateBattleCompletion();
    bool ValidateInitialState(FString& OutError) const;

    FOGTurnBattleState State;
    FOGCombatLog Log;
};
