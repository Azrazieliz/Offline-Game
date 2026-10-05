#pragma once

#include "CoreMinimal.h"
#include "Combat/OGCombatIdentityRules.h"
#include "Combat/OGCombatLog.h"
#include "Combat/OGCombatRankHooks.h"
#include "Combat/OGCombatTriggerRuntime.h"
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

    /** Preferred order: opening unit -> second-wave -> third-wave. */
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

class OFFLINEGAME_API FOGTurnBattle
{
public:
    bool Initialize(
        const FOGTurnBattleState& InitialState,
        FString& OutError);

    bool Initialize(
        const FOGTurnBattleState& InitialState,
        TArray<FOGCombatTriggerBinding> TriggerBindings,
        FOGCombatConditionEvaluator ConditionEvaluator,
        FString& OutError);

    bool Initialize(
        const FOGTurnBattleState& InitialState,
        TArray<FOGCombatTriggerBinding> TriggerBindings,
        FOGCombatConditionEvaluator ConditionEvaluator,
        FOGIdentityExclusivityContext IdentityContext,
        FOGRankSuppressionResolver RankResolver,
        FString& OutError);

    const FOGTurnBattleState& GetState() const { return State; }
    const FOGCombatLog& GetLog() const { return Log; }

    int32 SelectNextActingUnitIndex() const;

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
     * Finalizes a completed action after all OnDefeat/revival actions have been
     * resolved. Direct successors are preferred. Any still-empty battlefield
     * lane is then filled from other surviving reserves when possible.
     */
    bool ResolvePendingReplacements(FString& OutError);

    /** Triggered actions are resolved by the normal effect/action layer. */
    TArray<FOGQueuedTriggeredAction> DrainTriggeredActions();

    bool ResolveRankSuppressionMultiplier(
        const FOGEntityId& SourceUnitId,
        const FOGEntityId& TargetUnitId,
        FName ChannelId,
        int32& OutMultiplierBps,
        FString& OutError) const;

    /**
     * Call after a queued triggered action has fully applied its effects.
     * The last completed defeat-trigger action automatically closes the revival
     * window and performs succession/rebalancing.
     */
    bool CompleteTriggeredAction(
        int64 QueueSequence,
        FString& OutError);

private:
    FOGCombatUnitState* FindMutableUnit(const FOGEntityId& UnitId);
    const FOGCombatUnitState* FindUnit(const FOGEntityId& UnitId) const;
    const FOGTurnTeamState* FindTeam(int32 TeamIndex) const;
    const FOGTurnSuccessionLane* FindOriginLane(
        int32 TeamIndex,
        const FOGEntityId& UnitId,
        int32& OutUnitDepth) const;

    bool NormalizeOpeningLanes(FString& OutError);
    FOGCombatUnitState* FindPreferredDirectSuccessor(
        const FOGCombatUnitState& Defeated);
    FOGCombatUnitState* FindFallbackReserve(
        int32 TeamIndex);
    bool IsBattlefieldLaneOccupied(
        int32 TeamIndex,
        int32 BattlefieldLane) const;
    void PromoteIntoLane(
        FOGCombatUnitState& Unit,
        int32 BattlefieldLane,
        const TCHAR* Reason);

    void QueueTriggerEvent(
        FName EventType,
        const FOGEntityId& SourceUnitId,
        const FOGEntityId& TargetUnitId,
        const FOGContentId& SkillId,
        int64 SourceSequence);

    void FlushTriggerRuntimeQueueToBattleQueue();
    void EvaluateBattleCompletion();
    bool ValidateInitialState(FString& OutError) const;

    FOGTurnBattleState State;
    FOGCombatLog Log;
    FOGCombatTriggerRuntime TriggerRuntime;
    FOGIdentityExclusivityContext IdentityContext;
    FOGRankSuppressionResolver RankSuppressionResolver;
    TArray<FOGQueuedTriggeredAction> PendingTriggeredActions;
    TSet<int64> OutstandingTriggeredActionSequences;
    TArray<FOGEntityId> PendingDefeatedUnitIds;
};
