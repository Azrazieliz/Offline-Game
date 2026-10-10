#pragma once

#include "CoreMinimal.h"
#include "World/OGStartingRegionPresentation.h"
#include "OGKoikatsuPlayableCharacter.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EOGKKVisualState : uint8
{
    Idle, Walk, Run, Jump, Fall, Land, TurnLeft, TurnRight, Dodge, Action
};

// Reuses the already authored 19 skeletal mesh components in the fixture
// Blueprint. Never drops or reimports the original full-fidelity meshes.
UCLASS(Blueprintable)
class OFFLINEGAME_API AOGKoikatsuPlayableCharacter : public AOGWorldPrototypeCharacter
{
    GENERATED_BODY()
public:
    AOGKoikatsuPlayableCharacter();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Koikatsu|Motion")
    float WalkThresholdCmPerSecond = 7.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Koikatsu|Motion")
    float RunThresholdCmPerSecond = 430.f;

    UFUNCTION(BlueprintCallable, Category="Koikatsu|Facial")
    void SetExpressionMorph(FName MorphName, float Weight);

    UFUNCTION(BlueprintCallable, Category="Koikatsu|Motion")
    void TriggerVisualAction(float DurationSeconds = 0.5f);

    UFUNCTION(BlueprintCallable, Category="Koikatsu|Motion")
    void OverrideVisualState(EOGKKVisualState State, bool bEnabled);

    UFUNCTION(BlueprintPure, Category="Koikatsu|Motion")
    EOGKKVisualState GetKoikatsuVisualState() const { return CurrentState; }

    UFUNCTION(BlueprintPure, Category="Koikatsu|Motion")
    int32 GetKoikatsuVisualPartCount() const { return NativeParts.Num(); }

    virtual void Landed(const FHitResult& Hit) override;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<USkeletalMeshComponent>> NativeParts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UAnimSequence>> LoadedClips;

    TArray<int32> SourceMeshIds;
    TArray<int32> SourcePrimitiveIds;
    TArray<TObjectPtr<UAnimSequence>> ActiveClips;

    EOGKKVisualState CurrentState = EOGKKVisualState::Idle;
    EOGKKVisualState ForcedState = EOGKKVisualState::Idle;
    bool bOverride = false;
    float LandUntilTime = 0.f;
    float ActionUntilTime = 0.f;
    float LastYaw = 0.f;
    float YawDegreesPerSecond = 0.f;

    EOGKKVisualState ComputeState() const;
    void ApplyState(EOGKKVisualState State);
    UAnimSequence* ResolveNativeClip(int32 Part, EOGKKVisualState State);
    static bool ParsePartId(const FString& Name, int32& Mesh, int32& Primitive);
    static bool Loops(EOGKKVisualState State);
};
