#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OGDiagnosticAnimationPresentation.generated.h"

class UAnimInstance;
class UAnimSequence;
class USkeletalMeshComponent;
class UPoseableMeshComponent;
class USkeletalMesh;

USTRUCT(BlueprintType)
struct FOGDiagnosticAnimationClip
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UAnimSequence> Sequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bLoop = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="4.0"))
    float PlayRate = 1.0f;
};

/** Disposable content binding only; never owns combat/traversal/death state. */
UCLASS(ClassGroup=(OfflineGame), meta=(BlueprintSpawnableComponent))
class OFFLINEGAME_API UOGDiagnosticAnimationPresentation : public UActorComponent
{
    GENERATED_BODY()

public:
    UOGDiagnosticAnimationPresentation();

    UFUNCTION(BlueprintCallable, Category="OfflineGame|DiagnosticAnimation")
    void BindMesh(USkeletalMeshComponent* Mesh);

    // Release all temporary presentation ownership before another runtime stages
    // this physical actor. Unbind also releases mesh/driver references.
    UFUNCTION(BlueprintCallable, Category="OfflineGame|DiagnosticAnimation")
    void ResetPresentation();

    UFUNCTION(BlueprintCallable, Category="OfflineGame|DiagnosticAnimation")
    void UnbindMesh();

    UFUNCTION(BlueprintPure, Category="OfflineGame|DiagnosticAnimation")
    bool HasCompatibleBinding(FName Id) const;

    // Compatible authored clips win. Recognized unbound diagnostic states use a
    // neutral geometry-derived pose overlay; None restores captured locomotion.
    // A Death binding can have bLoop=false: its final pose holds until state exits.
    // Death preempts the presentation timer; physical defeat remains actor-owned.
    UFUNCTION(BlueprintCallable, Category="OfflineGame|DiagnosticAnimation")
    void SetPresentedState(FName State);

    // Call only after an existing runtime accepts an action/interaction/damage.
    // Returns a token to clear on accepted cancellation; 0 means no usable presentation.
    UFUNCTION(BlueprintCallable, Category="OfflineGame|DiagnosticAnimation")
    int32 PresentTimedAction(FName Action, float PresentationSeconds);

    UFUNCTION(BlueprintCallable, Category="OfflineGame|DiagnosticAnimation")
    void ClearTimedAction(int32 Token);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OfflineGame|DiagnosticAnimation")
    TMap<FName, FOGDiagnosticAnimationClip> Clips;

    // Explicit opt-in exists only on this disposable diagnostic component.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OfflineGame|DiagnosticAnimation")
    bool bEnableProceduralDiagnosticPoses = true;

    UFUNCTION(BlueprintPure, Category="OfflineGame|DiagnosticAnimation")
    bool HasProceduralBinding(FName Id) const;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool PlayBoundClip(FName Id, bool bRestart);
    void RefreshState();
    void RestoreLocomotion();
    bool OwnsMeshDriver() const;
    bool PresentState(FName State, bool bRestart);
    bool BeginProceduralPose(FName State, bool bRestart);
    void EndProceduralPose(bool bDestroy = true);
    void UpdateProceduralPose(float DeltaTime);
    void ResolveBoneRoles();
    void RotateRole(FName Role, const FVector& ComponentAxis, float Radians);
    void AimRole(FName Role, FName Child, const FVector& Direction, float Strength);

    UPROPERTY(Transient)
    TObjectPtr<UPoseableMeshComponent> DiagnosticPoseMesh;

    TMap<FName, int32> BoneRoles;
    TWeakObjectPtr<USkeletalMesh> ResolvedRoleMesh;
    FName ProceduralState;
    float ProceduralSeconds = 0.0f;
    bool bOwnsSourceVisibility = false;
    bool bSavedSourceVisibility = true;
    uint8 SavedSourceTickPolicy = 0;
    bool bOwnsSourceTickPolicy = false;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> BoundMesh;

    UPROPERTY(Transient)
    TSubclassOf<UAnimInstance> CapturedLocomotionClass;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> PlayingSequence;

    FName PresentedState;
    int32 ActionToken = 0;
    int32 NextToken = 0;
    float ActionRemainingSeconds = 0.0f;
};