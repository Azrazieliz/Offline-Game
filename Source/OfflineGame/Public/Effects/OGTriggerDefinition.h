#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "OGTriggerDefinition.generated.h"

/**
 * A condition is interpreted by a registered predicate handler.
 *
 * Parameters are intentionally small scalar fields rather than arbitrary
 * executable code. Bespoke mechanics can register bespoke predicates.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGConditionDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName Predicate = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ContentOperand;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName NameOperand = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 IntegerOperand = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bInvert = false;
};

/**
 * Event-triggered action definition used by passives, reactions, entry effects,
 * counters, follow-ups, reserve mechanics and similar automatic behavior.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTriggerDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId TriggerId;

    /** e.g. OnBattleStart, OnHit, OnDamageReceived, OnDefeat, OnEnterReserve. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName EventType = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGConditionDefinition> Conditions;

    /**
     * Actions/effects resolved by content ID. A trigger never embeds Blueprint
     * execution order as authoritative game logic.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGContentId> ActionIds;
};
