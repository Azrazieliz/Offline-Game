#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "OGCombatLog.generated.h"

UENUM(BlueprintType)
enum class EOGCombatLogEventType : uint8
{
    BattleStarted,
    ActionDeclared,
    ActionResolved,
    TriggeredActionQueued,
    DamageApplied,
    HealingApplied,
    UnitDefeated,
    PresenceChanged,
    BattleEnded
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCombatLogEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Sequence = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGCombatLogEventType Type = EOGCombatLogEventType::ActionResolved;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceUnitId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TargetUnitId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SkillId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PayloadJson = TEXT("{}");
};

class OFFLINEGAME_API FOGCombatLog
{
public:
    void Append(FOGCombatLogEvent Event);

    const TArray<FOGCombatLogEvent>& GetEvents() const { return Events; }

    int64 GetNextSequence() const { return NextSequence; }

    void Reset()
    {
        Events.Reset();
        NextSequence = 0;
    }

private:
    TArray<FOGCombatLogEvent> Events;
    int64 NextSequence = 0;
};
