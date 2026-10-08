#pragma once

#include "Components/ActorComponent.h"
#include "Combat/OGCombatTypes.h"
#include "OGDiagnosticEffectHandler.h"
#include "OGDiagnosticEnemyStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOGDiagnosticEnemyDefeated);

// Enemy only: owner of its diagnostic combat snapshot. Player state remains
// authoritative in the existing party runtime, not duplicated into this component.
UCLASS(ClassGroup=(OfflineGame), meta=(BlueprintSpawnableComponent))
class OFFLINEGAME_API UOGDiagnosticEnemyStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    void Initialize(const FOGCombatUnitState& InSnapshot) { Snapshot = InSnapshot; }
    const FOGCombatUnitState& GetSnapshot() const { return Snapshot; }
    // Receive already-resolved shared math damage; never accepts arbitrary float HP.
    bool ApplyResolvedDamage(const FOGLargeNumber& Damage, FString& Error);
    FOGDiagnosticExposeState& GetExposure() { return Exposure; }
    const FOGDiagnosticExposeState& GetExposure() const { return Exposure; }
    UPROPERTY(BlueprintAssignable) FOnOGDiagnosticEnemyDefeated OnDefeated;
private:
    UPROPERTY(Transient) FOGCombatUnitState Snapshot;
    FOGDiagnosticExposeState Exposure;
};
