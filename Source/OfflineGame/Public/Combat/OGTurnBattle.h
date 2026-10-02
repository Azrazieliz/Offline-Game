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
struct OFFLINEGAME_API FOGTurnSuccessionLane
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 LaneIndex = 0;

    /** Opening unit -> second-wave successor -> third-wave successor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGEntityId> OrderedUnitIds;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTurnTeamState
{
    GENERATED_BODY()

    static constexpr int32 MaxRosterSize = 18;
    static constexpr int32 MaxActiveSize = 6;
    static constexpr int32 MaxLaneSize = 3;
    static constexpr int32 MaxLaneCount = 6;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 TeamIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGTurnSuccessionLane> Lanes;

    int32 GetRosterSize() const
    {
        int32 Count = 0;
        for (const FOGTurnSuccessionLane& Lane : Lanes)
        {
            Count += Lane.OrderedUnitIds.Num();
        }
        return Count;
    }
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
     *
     * Pending normal succession replacements are promoted after this action.
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

    /**
     * Promotes direct-lane successors for units defeated during the just-finished
     * action. Entry/passive mechanics may override this through explicit rules.
     */
    bool ResolvePendingReplacements(FString& OutError);

private:
    FOGCombatUnitState* FindMutableUnit(const FOGEntityId& UnitId);
    const FOGCombatUnitState* FindUnit(const FOGEntityId& UnitId) const;
    const FOGTurnTeamState* FindTeam(int32 TeamIndex) const;
    const FOGTurnSuccessionLane* FindLaneContaining(
        int32 TeamIndex,
        const FOGEntityId& UnitId,
        int32& OutUnitIndex) const;

    void EvaluateBattleCompletion();
    bool ValidateInitialState(FString& OutError) const;

    FOGTurnBattleState State;
    FOGCombatLog Log;
    TArray<FOGEntityId> PendingDefeatedUnitIds;
};
