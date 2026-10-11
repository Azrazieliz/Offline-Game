#pragma once

#include "CoreMinimal.h"
#include "World/OGStartingRegionPresentation.h"
#include "OGKoikatsuPlayableCharacter.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EOGKKVisualState : uint8
{
    Idle, Walk, Run, Jump, Fall, Land, TurnLeft, TurnRight, Dodge, Action, SwimIdle, Swim
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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Koikatsu|Motion")
    float MotionStateMinHoldSeconds = 0.16f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Koikatsu|Motion")
    float MotionSpeedSmoothing = 8.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Koikatsu|Motion")
    bool bSynchronizeNativePartPhases = true;

    UPROPERTY(BlueprintReadOnly, Category="Koikatsu|Motion")
    bool bAllPartsHaveNativeQualityV2 = false;

    UFUNCTION(BlueprintCallable, Category="Koikatsu|Facial")
    bool PulseExpressionMorph(FName MorphName, float PeakWeight = 1.f, float RiseSeconds = 0.14f, float HoldSeconds = 0.35f, float FadeSeconds = 0.20f);

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
    float SmoothedHorizontalSpeed = 0.f;
    float ActiveStateSeconds = 0.f;
    float GaitPhaseCycles = 0.f;
    float LastMotionSwitchTime = -100.f;

    struct FExpressionPulse
    {
        FName MorphName = NAME_None;
        float Peak = 0.f;
        float Start = 0.f;
        float Rise = 0.f;
        float Hold = 0.f;
        float Fade = 0.f;
    };
    TArray<FExpressionPulse> ExpressionPulses;

    EOGKKVisualState ComputeState() const;
    void ApplyState(EOGKKVisualState State);
    UAnimSequence* ResolveNativeClip(int32 Part, EOGKKVisualState State);
    static bool ParsePartId(const FString& Name, int32& Mesh, int32& Primitive);
    static bool Loops(EOGKKVisualState State);
    void SyncPartPhases(float DeltaSeconds);
    void UpdateExpressionPulses(float Now);
    bool IsQualityV2Complete() const;
    UAnimSequence* LoadNativeClip(int32 Part, EOGKKVisualState State, bool bQualityV2);
};
