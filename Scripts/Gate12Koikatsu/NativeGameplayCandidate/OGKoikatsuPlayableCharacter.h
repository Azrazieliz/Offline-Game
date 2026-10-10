#pragma once
#include "CoreMinimal.h"
#include "World/OGStartingRegionPresentation.h"
#include "OGKoikatsuPlayableCharacter.generated.h"

class UAnimSequence;
class USkeletalMesh;
class USkeletalMeshComponent;

// STAGED/UNCOMPILED ENGINEERING ADAPTER. Do not classify as native validated.
UENUM(BlueprintType)
enum class EOGKKMovementVisualState : uint8
{
    Idle, Walk, Run, Airborne, Dodge
};

USTRUCT(BlueprintType)
struct FOGKKImportedVisualPart
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Koikatsu|Quality")
    TObjectPtr<USkeletalMesh> Mesh = nullptr;

    // Every clip must use this exact imported skeleton, no compatibility guesses.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Koikatsu|Animations")
    TObjectPtr<UAnimSequence> Idle = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Koikatsu|Animations")
    TObjectPtr<UAnimSequence> Walk = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Koikatsu|Animations")
    TObjectPtr<UAnimSequence> Run = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Koikatsu|Animations")
    TObjectPtr<UAnimSequence> Airborne = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Koikatsu|Animations")
    TObjectPtr<UAnimSequence> Dodge = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Koikatsu|Quality")
    FName PartId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Koikatsu|Quality")
    bool bVisible = true;
};

// Derived from the game's existing mobile-input, movement and action pawn.
UCLASS(Blueprintable)
class OFFLINEGAME_API AOGKoikatsuPlayableCharacter : public AOGWorldPrototypeCharacter
{
    GENERATED_BODY()

public:
    AOGKoikatsuPlayableCharacter();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Koikatsu|Content")
    TArray<FOGKKImportedVisualPart> FullQualityParts;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Koikatsu|Alignment")
    FVector ImportedMeshOffset = FVector(0.0, 0.0, -96.0);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Koikatsu|Movement")
    float WalkThresholdCmPerSecond = 7.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Koikatsu|Movement")
    float RunThresholdCmPerSecond = 430.0f;

    UFUNCTION(BlueprintCallable, Category="Koikatsu|Facial")
    void SetExpressionMorph(FName Morph, float Weight);

    UFUNCTION(BlueprintCallable, Category="Koikatsu|Movement")
    void OverrideMotionState(EOGKKMovementVisualState NewState, bool bEnable);

    UFUNCTION(BlueprintPure, Category="Koikatsu|Movement")
    EOGKKMovementVisualState GetCurrentVisualState() const { return CurrentState; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<USkeletalMeshComponent>> NativeParts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UAnimSequence>> ActiveSequences;

    EOGKKMovementVisualState CurrentState = EOGKKMovementVisualState::Idle;
    EOGKKMovementVisualState ForcedState = EOGKKMovementVisualState::Idle;
    bool bForceState = false;

    EOGKKMovementVisualState ComputeMotionState() const;
    void ApplyMotionState(EOGKKMovementVisualState State);
    UAnimSequence* ResolveClip(const FOGKKImportedVisualPart& Part, EOGKKMovementVisualState State) const;
};
