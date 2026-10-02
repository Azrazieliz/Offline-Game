#pragma once

#include "CoreMinimal.h"
#include "Core/OGEntityId.h"
#include "Effects/OGTriggerDefinition.h"
#include "Combat/OGCombatTypes.h"
#include "OGCombatTriggerRuntime.generated.h"

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCombatTriggerBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerUnitId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGTriggerDefinition Trigger;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCombatTriggerContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName EventType = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceUnitId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TargetUnitId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SkillId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 SourceSequence = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGQueuedTriggeredAction
{
    GENERATED_BODY()

    /** Unique deterministic queue instance inside one battle. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 QueueSequence = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerUnitId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId TriggerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ActionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 SourceSequence = 0;
};

using FOGCombatConditionEvaluator =
    TFunction<bool(
        const FOGCombatTriggerBinding&,
        const FOGConditionDefinition&,
        const FOGCombatTriggerContext&)>;

class OFFLINEGAME_API FOGCombatTriggerRuntime
{
public:
    void SetBindings(TArray<FOGCombatTriggerBinding> InBindings);
    void SetConditionEvaluator(FOGCombatConditionEvaluator InEvaluator)
    {
        ConditionEvaluator = MoveTemp(InEvaluator);
    }

    void QueueForEvent(const FOGCombatTriggerContext& Context);
    TArray<FOGQueuedTriggeredAction> DrainQueuedActions();
    void Reset();

private:
    bool ConditionsPass(
        const FOGCombatTriggerBinding& Binding,
        const FOGCombatTriggerContext& Context) const;

    TArray<FOGCombatTriggerBinding> Bindings;
    TArray<FOGQueuedTriggeredAction> QueuedActions;
    FOGCombatConditionEvaluator ConditionEvaluator;
    int64 NextQueueSequence = 0;
};

namespace OGCombatEventNames
{
    inline FName BattleStart()
    {
        static const FName Value(TEXT("OnBattleStart"));
        return Value;
    }

    inline FName Defeat()
    {
        static const FName Value(TEXT("OnDefeat"));
        return Value;
    }

    inline FName Entry()
    {
        static const FName Value(TEXT("OnEntry"));
        return Value;
    }

    inline FName ReserveEntry()
    {
        static const FName Value(TEXT("OnEnterReserve"));
        return Value;
    }
}
