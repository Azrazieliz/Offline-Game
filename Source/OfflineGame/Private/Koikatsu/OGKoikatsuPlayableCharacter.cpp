#include "Koikatsu/OGKoikatsuPlayableCharacter.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
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
    bAllPartsHaveNativeQualityV2=IsQualityV2Complete();
    SmoothedHorizontalSpeed=GetVelocity().Size2D();
    ApplyState(EOGKKVisualState::Idle);
    UE_LOG(LogTemp, Display, TEXT("Gate12 KK MotionV2: native=%d complete_quality_clips=%d synced=%d"),
           NativeParts.Num(),bAllPartsHaveNativeQualityV2,bSynchronizeNativePartPhases);
    UE_LOG(LogTemp, Display, TEXT("Gate12 KK: bound %d true skeletal mesh parts to native action controller"),
           NativeParts.Num());
}

UAnimSequence* AOGKoikatsuPlayableCharacter::LoadNativeClip(
    int32 Index, EOGKKVisualState State, bool bQualityV2)
{
    if(!NativeParts.IsValidIndex(Index)||!SourceMeshIds.IsValidIndex(Index)||
       !SourcePrimitiveIds.IsValidIndex(Index)) return nullptr;
    USkeletalMeshComponent* Comp=NativeParts[Index];
    if(!Comp || !Comp->GetSkeletalMeshAsset()) return nullptr;
    const TCHAR* Name=TEXT("Idle");
    switch(State)
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
      case EOGKKVisualState::Swim: Name=TEXT("Swim"); break;
      case EOGKKVisualState::SwimIdle: Name=TEXT("SwimIdle"); break;
      default: break;
    }
    const FString Asset=bQualityV2
       ? FString::Printf(TEXT("AN_KK_V2_%s_M%02d_P%02d"),Name,SourceMeshIds[Index],SourcePrimitiveIds[Index])
       : State==EOGKKVisualState::Idle
         ? FString::Printf(TEXT("AN_KK_NativeRestSafe_M%02d_P%02d"),SourceMeshIds[Index],SourcePrimitiveIds[Index])
         : FString::Printf(TEXT("AN_KK_%s_M%02d_P%02d"),Name,SourceMeshIds[Index],SourcePrimitiveIds[Index]);
    const FString Folder=bQualityV2
       ? TEXT("/Game/Experimental/Gate12Koikatsu/Animation/NativeQualityV2")
       : State==EOGKKVisualState::Idle
         ? TEXT("/Game/Experimental/Gate12Koikatsu/Animation/NativeRestSafe")
         : TEXT("/Game/Experimental/Gate12Koikatsu/Animation/NativeActions");
    const FString Path=FString::Printf(TEXT("%s/%s.%s"),*Folder,*Asset,*Asset);
    UAnimSequence* Seq=LoadObject<UAnimSequence>(nullptr,*Path);
    if(Seq && Seq->GetSkeleton()!=Comp->GetSkeletalMeshAsset()->GetSkeleton())
    {
        UE_LOG(LogTemp, Error,TEXT("Gate12 KK MotionV2: exact-skeleton mismatch %s"),*Path);
        return nullptr;
    }
    return Seq;
}

bool AOGKoikatsuPlayableCharacter::IsQualityV2Complete() const
{
    if(NativeParts.Num()!=19) return false;
    for(int32 I=0;I<NativeParts.Num();++I)
        for(EOGKKVisualState S : {EOGKKVisualState::Walk,EOGKKVisualState::Run,
                                 EOGKKVisualState::Swim,EOGKKVisualState::SwimIdle})
            if(!const_cast<AOGKoikatsuPlayableCharacter*>(this)->LoadNativeClip(I,S,true))
                return false;
    return true;
}

UAnimSequence* AOGKoikatsuPlayableCharacter::ResolveNativeClip(int32 Index,EOGKKVisualState State)
{
    UAnimSequence* Clip=nullptr;
    if(bAllPartsHaveNativeQualityV2 &&
       (State==EOGKKVisualState::Walk||State==EOGKKVisualState::Run||
        State==EOGKKVisualState::Swim||State==EOGKKVisualState::SwimIdle))
        Clip=LoadNativeClip(Index,State,true);
    // Old fixture lacks a swim clip: a safe rest pose is preferable to
    // erroneously cycling through giant ground-running strides underwater.
    if(!Clip && (State==EOGKKVisualState::Swim||State==EOGKKVisualState::SwimIdle))
        Clip=LoadNativeClip(Index,EOGKKVisualState::Idle,false);
    if(!Clip) Clip=LoadNativeClip(Index,State,false);
    if(!Clip)
    {
        UE_LOG(LogTemp,Warning,TEXT("Gate12 KK MotionV2: missing exact clip part=%d state=%d"),
               Index,static_cast<int32>(State));
        return nullptr;
    }
    LoadedClips.AddUnique(Clip);
    return Clip;
}

bool AOGKoikatsuPlayableCharacter::Loops(EOGKKVisualState State)
{
    return State == EOGKKVisualState::Idle || State == EOGKKVisualState::Walk ||
           State == EOGKKVisualState::Run || State == EOGKKVisualState::Fall ||
           State == EOGKKVisualState::TurnLeft || State == EOGKKVisualState::TurnRight ||
           State == EOGKKVisualState::Swim || State == EOGKKVisualState::SwimIdle;
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
        if(bSynchronizeNativePartPhases)
            if(UAnimSingleNodeInstance* Inst=Comp->GetSingleNodeInstance())
                Inst->SetPlaying(false);
        ActiveClips[Index]=Seq;
        ++Replaced;
    }
    CurrentState=Next;
    ActiveStateSeconds=0.f;
    LastMotionSwitchTime=GetWorld()?GetWorld()->GetTimeSeconds():0.f;
    UE_LOG(LogTemp, Display, TEXT("Gate12 KK: state=%d clips_transitioned=%d/%d"),
           static_cast<int32>(Next),Replaced,NativeParts.Num());
}

EOGKKVisualState AOGKoikatsuPlayableCharacter::ComputeState() const
{
    if (bOverride) return ForcedState;
    const UCharacterMovementComponent* Movement=GetCharacterMovement();
    if(Movement && Movement->IsSwimming())
        return SmoothedHorizontalSpeed<=WalkThresholdCmPerSecond
            ? EOGKKVisualState::SwimIdle : EOGKKVisualState::Swim;
    if (IsDodging()) return EOGKKVisualState::Dodge;
    const UWorld* World=GetWorld();
    const float Now=World ? World->GetTimeSeconds() : 0.f;
    if (Now<ActionUntilTime) return EOGKKVisualState::Action;
    if (Movement && Movement->IsFalling())
        return GetVelocity().Z>5.f ? EOGKKVisualState::Jump : EOGKKVisualState::Fall;
    if (Now<LandUntilTime) return EOGKKVisualState::Land;
    const float Speed=SmoothedHorizontalSpeed;
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
    SmoothedHorizontalSpeed=FMath::FInterpTo(SmoothedHorizontalSpeed,
        GetVelocity().Size2D(),DeltaSeconds,FMath::Max(0.5f,MotionSpeedSmoothing));
    const EOGKKVisualState Next=ComputeState();
    const bool bGaitCurrent=CurrentState==EOGKKVisualState::Walk||CurrentState==EOGKKVisualState::Run;
    const bool bGaitNext=Next==EOGKKVisualState::Walk||Next==EOGKKVisualState::Run;
    const float Now=GetWorld()?GetWorld()->GetTimeSeconds():0.f;
    if(Next!=CurrentState && (!bGaitCurrent||!bGaitNext||
        Now-LastMotionSwitchTime>=MotionStateMinHoldSeconds))
        ApplyState(Next);
    SyncPartPhases(DeltaSeconds);
    UpdateExpressionPulses(Now);
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
    ExpressionPulses.Reset();
    Super::EndPlay(Reason);
}


void AOGKoikatsuPlayableCharacter::SyncPartPhases(float DeltaSeconds)
{
    if(!bSynchronizeNativePartPhases||DeltaSeconds<=0.f||
       NativeParts.Num()!=ActiveClips.Num()||NativeParts.Num()!=19) return;
    ActiveStateSeconds+=DeltaSeconds;
    const bool bGait=CurrentState==EOGKKVisualState::Walk||CurrentState==EOGKKVisualState::Run;
    if(bGait)
    {
        const float Cycle=CurrentState==EOGKKVisualState::Run?0.8f:1.0f;
        const float Scale=FMath::Clamp(
            SmoothedHorizontalSpeed/(CurrentState==EOGKKVisualState::Run?650.f:350.f),0.65f,1.35f);
        GaitPhaseCycles=FMath::Fmod(GaitPhaseCycles+DeltaSeconds*Scale/Cycle,1.f);
    }
    const bool bSwim=CurrentState==EOGKKVisualState::Swim||CurrentState==EOGKKVisualState::SwimIdle;
    const float SwimPhase=bSwim?FMath::Fmod(ActiveStateSeconds/
        (CurrentState==EOGKKVisualState::Swim?1.2f:1.8f),1.f):0.f;
    for(int32 I=0;I<NativeParts.Num();++I)
    {
        USkeletalMeshComponent* Comp=NativeParts[I];
        UAnimSequence* Seq=ActiveClips[I];
        if(!Comp||!Seq) continue;
        if(UAnimSingleNodeInstance* Inst=Comp->GetSingleNodeInstance())
            Inst->SetPlaying(false);
        const float Length=FMath::Max(Seq->GetPlayLength(),0.01f);
        const float Time=bGait?GaitPhaseCycles*Length:
            bSwim?SwimPhase*Length:
            Loops(CurrentState)?FMath::Fmod(ActiveStateSeconds,Length):
                FMath::Min(ActiveStateSeconds,Length-0.001f);
        Comp->SetPosition(Time,false);
    }
}

bool AOGKoikatsuPlayableCharacter::PulseExpressionMorph(
    FName Name,float PeakWeight,float RiseSeconds,float HoldSeconds,float FadeSeconds)
{
    if(Name.IsNone()||NativeParts.Num()!=19) return false;
    int32 Matches=0;
    for(USkeletalMeshComponent* Comp:NativeParts)
        if(Comp&&Comp->GetSkeletalMeshAsset()&&
           Comp->GetSkeletalMeshAsset()->FindMorphTarget(Name))
            ++Matches;
    if(Matches==0) return false;
    ExpressionPulses.RemoveAll([Name](const FExpressionPulse& P){return P.MorphName==Name;});
    FExpressionPulse P;
    P.MorphName=Name;
    P.Peak=FMath::Clamp(PeakWeight,0.f,1.f);
    P.Start=GetWorld()?GetWorld()->GetTimeSeconds():0.f;
    P.Rise=FMath::Max(RiseSeconds,0.01f);
    P.Hold=FMath::Max(HoldSeconds,0.f);
    P.Fade=FMath::Max(FadeSeconds,0.01f);
    ExpressionPulses.Add(P);
    UE_LOG(LogTemp,Display,TEXT("Gate12 KK MotionV2: morph pulse %s real_native_matches=%d"),
           *Name.ToString(),Matches);
    return true;
}

void AOGKoikatsuPlayableCharacter::UpdateExpressionPulses(float Now)
{
    for(int32 I=ExpressionPulses.Num()-1;I>=0;--I)
    {
        const FExpressionPulse& P=ExpressionPulses[I];
        const float T=FMath::Max(0.f,Now-P.Start);
        const float Total=P.Rise+P.Hold+P.Fade;
        float Value=0.f;
        if(T<P.Rise) Value=P.Peak*FMath::SmoothStep(0.f,P.Rise,T);
        else if(T<P.Rise+P.Hold) Value=P.Peak;
        else if(T<Total)
            Value=P.Peak*(1.f-FMath::SmoothStep(P.Rise+P.Hold,Total,T));
        SetExpressionMorph(P.MorphName,Value);
        if(T>=Total) ExpressionPulses.RemoveAtSwap(I);
    }
}
