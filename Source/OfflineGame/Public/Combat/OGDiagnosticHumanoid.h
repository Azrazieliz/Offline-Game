#pragma once

#include "GameFramework/Character.h"
#include "World/OGWorldModeInterfaces.h"
#include "OGDiagnosticEnemyStateComponent.h"
#include "OGDiagnosticHumanoid.generated.h"

class UOGDiagnosticCombatComponent;

UCLASS()
class OFFLINEGAME_API AOGDiagnosticHumanoid : public ACharacter,
    public IOGWorldTargetable, public IOGWorldInteractable
{
    GENERATED_BODY()
public:
    AOGDiagnosticHumanoid();
    virtual void Tick(float Delta) override;
    void Initialize(UOGDiagnosticCombatComponent& Driver, const FOGCombatUnitState& Unit, bool bTurnStation);
    UOGDiagnosticEnemyStateComponent* GetCombatState() const { return State; }
    bool IsTelegraphing() const { return bTelegraph; }
    bool IsTurnStation() const { return bStation; }
    bool HasLineOfSight(AActor* Other) const;
    void ShowAttackPose(double Seconds = 0.22);
    virtual bool CanBeTargeted_Implementation(AActor* Requester) const override;
    virtual FVector GetTargetPoint_Implementation(AActor* Requester) const override;
    virtual bool CanInteract_Implementation(AActor* Interactor) const override;
    virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
    virtual void Interact_Implementation(AActor* Interactor) override;
private:
    UFUNCTION() void HandleDefeat();
    void FreezeDefeatedRagdoll();
    void SetBodyTint(FLinearColor Color);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UOGDiagnosticEnemyStateComponent> State;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> BodyMaterial;
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UOGDiagnosticAnimationPresentation> AnimationPresentation;
    TWeakObjectPtr<UOGDiagnosticCombatComponent> Driver;
    bool bStation = false;
    bool bTelegraph = false;
    bool bDefeatPresented = false;
    FTimerHandle DefeatFreezeTimer;
    double AttackAt = 0.0;
    double NextStrikeAt = 0.0;
    double PoseEndsAt = 0.0;
};
