#include "Koikatsu/OGKoikatsuPlayableCharacter.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

AOGKoikatsuPlayableCharacter::AOGKoikatsuPlayableCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
}

bool AOGKoikatsuPlayableCharacter::ParsePartId(
    const FString& Name, int32& Mesh, int32& Primitive)
{
    if (!Name.StartsWith(TEXT("KK_Full_M"))) return false;
    FString Left, Right, MeshString, PrimitiveString;
    if (!Name.Split(TEXT("_M"), &Left, &Right)) return false;
    if (!Right.Split(TEXT("P"), &MeshString, &PrimitiveString)) return false;
    // UE5 SCS component templates can retain a _GEN_VARIABLE suffix;
    // runtime instances and source receipts use the public primitive ID.
    PrimitiveString.RemoveFromEnd(TEXT("_GEN_VARIABLE"));
    if (!MeshString.IsNumeric() || !PrimitiveString.IsNumeric()) return false;
    Mesh = FCString::Atoi(*MeshString);
    Primitive = FCString::Atoi(*PrimitiveString);
    return Mesh >= 0 && Mesh <= 99 && Primitive >= 0 && Primitive <= 99;
}

void AOGKoikatsuPlayableCharacter::BeginPlay()
{
    Super::BeginPlay();
    TArray<USkeletalMeshComponent*> Existing;
    GetComponents<USkeletalMeshComponent>(Existing);
    for (USkeletalMeshComponent* Comp : Existing)
    {
        int32 MeshIndex=-1, PrimitiveIndex=-1;
        if (!Comp || !ParsePartId(Comp->GetName(), MeshIndex, PrimitiveIndex)) continue;
        if (!Comp->GetSkeletalMeshAsset() || !Comp->GetSkeletalMeshAsset()->GetSkeleton())
        {
            UE_LOG(LogTemp, Error, TEXT("Gate12 KK: missing imported skeleton for %s"), *Comp->GetName());
            continue;
        }
        NativeParts.Add(Comp);
        SourceMeshIds.Add(MeshIndex);
        SourcePrimitiveIds.Add(PrimitiveIndex);
        ActiveClips.Add(nullptr);
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    }
    // Avoid a second copy of each mesh: actual Blueprint already contains 19.
    if (NativeParts.Num() != 19)
    {
        UE_LOG(LogTemp, Error, TEXT("Gate12 KK: expected 19 exact imported Blueprint mesh parts, found %d"),
               NativeParts.Num());
        return;
    }
    if (GetMesh()) GetMesh()->SetHiddenInGame(true);
    LastYaw = GetActorRotation().Yaw;
    ApplyState(EOGKKVisualState::Idle);
    UE_LOG(LogTemp, Display, TEXT("Gate12 KK: bound %d true skeletal mesh parts to native action controller"),
           NativeParts.Num());
}

UAnimSequence* AOGKoikatsuPlayableCharacter::ResolveNativeClip(int32 Index, EOGKKVisualState State)
{
    if (!NativeParts.IsValidIndex(Index) || !SourceMeshIds.IsValidIndex(Index) ||
        !SourcePrimitiveIds.IsValidIndex(Index)) return nullptr;
    USkeletalMeshComponent* Comp = NativeParts[Index];
    if (!Comp || !Comp->GetSkeletalMeshAsset()) return nullptr;

    const TCHAR* Name = TEXT("Idle");
    switch (State)
    {
      case EOGKKVisualState::Walk: Name=TEXT("Walk"); break;
      case EOGKKVisualState::Run: Name=TEXT("Run"); break;
      case EOGKKVisualState::Jump: Name=TEXT("Jump"); break;
      case EOGKKVisualState::Fall: Name=TEXT("Fall"); break;
      case EOGKKVisualState::Land: Name=TEXT("Land"); break;
      case EOGKKVisualState::TurnLeft: Name=TEXT("TurnLeft"); break;
      case EOGKKVisualState::TurnRight: Name=TEXT("TurnRight"); break;
      case EOGKKVisualState::Dodge: Name=TEXT("Dodge"); break;
      case EOGKKVisualState::Action: Name=TEXT("Action"); break;
      default: break;
    }
    const FString Asset = State == EOGKKVisualState::Idle
        ? FString::Printf(TEXT("AN_KK_NativeRestSafe_M%02d_P%02d"),
                          SourceMeshIds[Index],SourcePrimitiveIds[Index])
        : FString::Printf(TEXT("AN_KK_%s_M%02d_P%02d"),
                          Name,SourceMeshIds[Index],SourcePrimitiveIds[Index]);
    const FString Folder = State == EOGKKVisualState::Idle
        ? TEXT("/Game/Experimental/Gate12Koikatsu/Animation/NativeRestSafe")
        : TEXT("/Game/Experimental/Gate12Koikatsu/Animation/NativeActions");
    const FString Path = FString::Printf(TEXT("%s/%s.%s"),*Folder,*Asset,*Asset);
    UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr,*Path);
    if (!Sequence)
    {
        UE_LOG(LogTemp, Warning, TEXT("Gate12 KK: action clip missing, no pretend state animation: %s"),*Path);
        return nullptr;
    }
    if (Sequence->GetSkeleton()!=Comp->GetSkeletalMeshAsset()->GetSkeleton())
    {
        UE_LOG(LogTemp, Error, TEXT("Gate12 KK: native clip skeleton mismatch: %s"),*Path);
        return nullptr;
    }
    LoadedClips.AddUnique(Sequence); // GC-safe runtime retention.
    return Sequence;
}

bool AOGKoikatsuPlayableCharacter::Loops(EOGKKVisualState State)
{
    return State == EOGKKVisualState::Idle || State == EOGKKVisualState::Walk ||
           State == EOGKKVisualState::Run || State == EOGKKVisualState::Fall ||
           State == EOGKKVisualState::TurnLeft || State == EOGKKVisualState::TurnRight;
}

void AOGKoikatsuPlayableCharacter::ApplyState(EOGKKVisualState Next)
{
    if (NativeParts.Num()!=19) return;
    int32 Replaced=0;
    for (int32 Index=0;Index<NativeParts.Num();++Index)
    {
        USkeletalMeshComponent* Comp=NativeParts[Index];
        if (!Comp) continue;
        UAnimSequence* Seq=ResolveNativeClip(Index,Next);
        if (!Seq) continue;
        if (ActiveClips[Index]==Seq) continue;
        Comp->PlayAnimation(Seq,Loops(Next));
        ActiveClips[Index]=Seq;
        ++Replaced;
    }
    CurrentState=Next;
    UE_LOG(LogTemp, Display, TEXT("Gate12 KK: state=%d clips_transitioned=%d/%d"),
           static_cast<int32>(Next),Replaced,NativeParts.Num());
}

EOGKKVisualState AOGKoikatsuPlayableCharacter::ComputeState() const
{
    if (bOverride) return ForcedState;
    if (IsDodging()) return EOGKKVisualState::Dodge;
    const UWorld* World=GetWorld();
    const float Now=World ? World->GetTimeSeconds() : 0.f;
    if (Now<ActionUntilTime) return EOGKKVisualState::Action;
    const UCharacterMovementComponent* Movement=GetCharacterMovement();
    if (Movement && Movement->IsFalling())
        return GetVelocity().Z>5.f ? EOGKKVisualState::Jump : EOGKKVisualState::Fall;
    if (Now<LandUntilTime) return EOGKKVisualState::Land;
    const float Speed=GetVelocity().Size2D();
    if (Speed<=WalkThresholdCmPerSecond)
    {
        if (YawDegreesPerSecond>30.f) return EOGKKVisualState::TurnRight;
        if (YawDegreesPerSecond< -30.f) return EOGKKVisualState::TurnLeft;
        return EOGKKVisualState::Idle;
    }
    if (IsSprinting() || Speed>=RunThresholdCmPerSecond) return EOGKKVisualState::Run;
    return EOGKKVisualState::Walk;
}

void AOGKoikatsuPlayableCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Yaw=GetActorRotation().Yaw;
    YawDegreesPerSecond=DeltaSeconds>0.0001f
        ? FMath::FindDeltaAngleDegrees(LastYaw,Yaw)/DeltaSeconds : 0.f;
    LastYaw=Yaw;
    const EOGKKVisualState Next=ComputeState();
    if (Next!=CurrentState) ApplyState(Next);
}

void AOGKoikatsuPlayableCharacter::Landed(const FHitResult& Hit)
{
    Super::Landed(Hit);
    if (UWorld* World=GetWorld()) LandUntilTime=World->GetTimeSeconds()+0.45f;
}

void AOGKoikatsuPlayableCharacter::TriggerVisualAction(float DurationSeconds)
{
    if (UWorld* World=GetWorld())
        ActionUntilTime=World->GetTimeSeconds()+FMath::Clamp(DurationSeconds,0.05f,5.f);
}

void AOGKoikatsuPlayableCharacter::OverrideVisualState(EOGKKVisualState State,bool bEnabled)
{
    ForcedState=State;
    bOverride=bEnabled;
    ApplyState(ComputeState());
}

void AOGKoikatsuPlayableCharacter::SetExpressionMorph(FName Name,float Weight)
{
    const float Clamped=FMath::Clamp(Weight,0.f,1.f);
    for (USkeletalMeshComponent* Comp : NativeParts)
    {
        if (Comp && Comp->GetSkeletalMeshAsset() &&
            Comp->GetSkeletalMeshAsset()->FindMorphTarget(Name))
            Comp->SetMorphTarget(Name,Clamped);
    }
}

void AOGKoikatsuPlayableCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    NativeParts.Reset();
    ActiveClips.Reset();
    LoadedClips.Reset();
    Super::EndPlay(Reason);
}
