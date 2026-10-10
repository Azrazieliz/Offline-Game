// Source-only engineering candidate. NOT compiled. Keep outside the active Source tree.
#include "OGKoikatsuPlayableCharacter.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"

AOGKoikatsuPlayableCharacter::AOGKoikatsuPlayableCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AOGKoikatsuPlayableCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (FullQualityParts.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("KK: No imported parts configured, keeping frozen native body"));
        return;
    }

    NativeParts.Reserve(FullQualityParts.Num());
    ActiveSequences.Reserve(FullQualityParts.Num());
    for (int32 Index = 0; Index < FullQualityParts.Num(); ++Index)
    {
        const FOGKKImportedVisualPart& Definition = FullQualityParts[Index];
        if (!Definition.Mesh)
        {
            UE_LOG(LogTemp, Warning, TEXT("KK: Null mesh part %d"), Index);
            NativeParts.Add(nullptr); ActiveSequences.Add(nullptr);
            continue;
        }

        USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(this);
        if (!Comp)
        {
            NativeParts.Add(nullptr); ActiveSequences.Add(nullptr);
            continue;
        }
        Comp->SetupAttachment(GetCapsuleComponent());
        Comp->SetRelativeLocation(ImportedMeshOffset);
        Comp->SetRelativeRotation(FRotator::ZeroRotator);
        Comp->SetRelativeScale3D(FVector::OneVector);
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetSkeletalMesh(Definition.Mesh);
        Comp->SetVisibility(Definition.bVisible);
        Comp->SetHiddenInGame(!Definition.bVisible);
        Comp->SetCastShadow(Definition.bVisible);
        Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        AddInstanceComponent(Comp);
        Comp->RegisterComponent();

        NativeParts.Add(Comp);
        ActiveSequences.Add(nullptr);
    }

    // Preserve frozen diagnostics underneath; only hide the proof mannequin visually.
    if (GetMesh()) GetMesh()->SetHiddenInGame(true);
    ApplyMotionState(EOGKKMovementVisualState::Idle);
}

UAnimSequence* AOGKoikatsuPlayableCharacter::ResolveClip(
    const FOGKKImportedVisualPart& Part, EOGKKMovementVisualState State) const
{
    UAnimSequence* Requested = nullptr;
    switch (State)
    {
        case EOGKKMovementVisualState::Walk: Requested = Part.Walk; break;
        case EOGKKMovementVisualState::Run: Requested = Part.Run ? Part.Run : Part.Walk; break;
        case EOGKKMovementVisualState::Airborne: Requested = Part.Airborne; break;
        case EOGKKMovementVisualState::Dodge: Requested = Part.Dodge; break;
        default: break;
    }
    if (!Requested) Requested = Part.Idle;
    if (!Requested || !Part.Mesh || Requested->GetSkeleton() != Part.Mesh->GetSkeleton())
    {
        return nullptr; // Never force a foreign rest pose onto a separate imported skeleton.
    }
    return Requested;
}

EOGKKMovementVisualState AOGKoikatsuPlayableCharacter::ComputeMotionState() const
{
    if (bForceState) return ForcedState;
    const UCharacterMovementComponent* Move = GetCharacterMovement();
    if (Move && Move->IsFalling()) return EOGKKMovementVisualState::Airborne;
    if (IsDodging()) return EOGKKMovementVisualState::Dodge;
    const float Speed = GetVelocity().Size2D();
    if (Speed <= WalkThresholdCmPerSecond) return EOGKKMovementVisualState::Idle;
    if (IsSprinting() || Speed >= RunThresholdCmPerSecond) return EOGKKMovementVisualState::Run;
    return EOGKKMovementVisualState::Walk;
}

void AOGKoikatsuPlayableCharacter::ApplyMotionState(EOGKKMovementVisualState NewState)
{
    CurrentState = NewState;
    for (int32 Index = 0; Index < NativeParts.Num(); ++Index)
    {
        USkeletalMeshComponent* Comp = NativeParts[Index];
        if (!Comp || !FullQualityParts.IsValidIndex(Index)) continue;
        UAnimSequence* Clip = ResolveClip(FullQualityParts[Index], NewState);
        if (!Clip || ActiveSequences[Index] == Clip) continue;
        Comp->PlayAnimation(Clip, true);
        ActiveSequences[Index] = Clip;
    }
}

void AOGKoikatsuPlayableCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const EOGKKMovementVisualState State = ComputeMotionState();
    if (State != CurrentState) ApplyMotionState(State);
}

void AOGKoikatsuPlayableCharacter::OverrideMotionState(
    EOGKKMovementVisualState NewState, bool bEnable)
{
    bForceState = bEnable;
    ForcedState = NewState;
    ApplyMotionState(ComputeMotionState());
}

void AOGKoikatsuPlayableCharacter::SetExpressionMorph(FName Morph, float Weight)
{
    const float SafeWeight = FMath::Clamp(Weight, 0.f, 1.f);
    for (USkeletalMeshComponent* Comp : NativeParts)
    {
        if (!Comp) continue;
        const USkeletalMesh* Mesh = Comp->GetSkeletalMeshAsset();
        if (Mesh && Mesh->FindMorphTarget(Morph))
        {
            Comp->SetMorphTarget(Morph, SafeWeight);
        }
    }
}

void AOGKoikatsuPlayableCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    NativeParts.Empty();
    ActiveSequences.Empty();
    Super::EndPlay(Reason);
}
