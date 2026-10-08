#include "Animation/OGDiagnosticAnimationPresentation.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/Actor.h"
#include <initializer_list>
#include "Engine/SkeletalMesh.h"
#include "UObject/ConstructorHelpers.h"

UOGDiagnosticAnimationPresentation::UOGDiagnosticAnimationPresentation()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    // These two packages exist on the installed TutorialTPP diagnostic rig.
    // Faster cadence reuses the actual walk sequence, never a fabricated run clip.
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(
        TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle.Tutorial_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Walk(
        TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Walk_Fwd.Tutorial_Walk_Fwd"));
    if (Idle.Succeeded())
    {
        FOGDiagnosticAnimationClip Clip; Clip.Sequence = Idle.Object; Clips.Add(TEXT("Idle"), Clip);
    }
    if (Walk.Succeeded())
    {
        FOGDiagnosticAnimationClip Clip; Clip.Sequence = Walk.Object;
        Clips.Add(TEXT("Walk"), Clip);
        Clip.PlayRate = 1.4f; Clips.Add(TEXT("Run"), Clip);
        Clip.PlayRate = 1.9f; Clips.Add(TEXT("Sprint"), Clip);
    }
    // Missing diagnostic actions use neutral joint poses, not idle clip aliases.
    // This fallback never supplies production animation or gameplay authority.
}

void UOGDiagnosticAnimationPresentation::BindMesh(USkeletalMeshComponent* Mesh)
{
    EndProceduralPose(true);
    RestoreLocomotion();
    if (BoundMesh) RemoveTickPrerequisiteComponent(BoundMesh);
    BoundMesh = nullptr;
    CapturedLocomotionClass = nullptr;
    BoneRoles.Reset();
    ActionToken = 0;
    ActionRemainingSeconds = 0.0f;
    if (!Mesh || Mesh->GetAnimationMode() != EAnimationMode::AnimationBlueprint ||
        !Mesh->GetAnimClass())
    {
        return; // Cannot safely restore an unknown content/runtime configuration.
    }
    BoundMesh = Mesh;
    CapturedLocomotionClass = Mesh->GetAnimClass();
    AddTickPrerequisiteComponent(Mesh);
    ResolveBoneRoles();
    RefreshState();
}

bool UOGDiagnosticAnimationPresentation::HasCompatibleBinding(FName Id) const
{
    const FOGDiagnosticAnimationClip* Clip = Clips.Find(Id);
    return BoundMesh && Clip && Clip->Sequence && BoundMesh->GetSkeletalMeshAsset() &&
        Clip->Sequence->GetSkeleton() == BoundMesh->GetSkeletalMeshAsset()->GetSkeleton();
}

bool UOGDiagnosticAnimationPresentation::PlayBoundClip(FName Id, bool bRestart)
{
    const FOGDiagnosticAnimationClip* Clip = Clips.Find(Id);
    if (!OwnsMeshDriver() || !BoundMesh || !Clip || !Clip->Sequence || !CapturedLocomotionClass ||
        !BoundMesh->GetSkeletalMeshAsset() ||
        Clip->Sequence->GetSkeleton() != BoundMesh->GetSkeletalMeshAsset()->GetSkeleton())
    {
        return false;
    }
    EndProceduralPose();
    if (!bRestart && PlayingSequence == Clip->Sequence &&
        BoundMesh->GetAnimationMode() == EAnimationMode::AnimationSingleNode)
    {
        BoundMesh->SetPlayRate(FMath::Clamp(Clip->PlayRate, 0.1f, 4.0f));
        return true;
    }
    // PlayAnimation switches only the mesh animation driver. Actor/capsule
    // transform, facing, proportions, movement and gameplay timing are untouched.
    BoundMesh->PlayAnimation(Clip->Sequence, Clip->bLoop);
    BoundMesh->SetPlayRate(FMath::Clamp(Clip->PlayRate, 0.1f, 4.0f));
    PlayingSequence = Clip->Sequence;
    return true;
}

void UOGDiagnosticAnimationPresentation::RestoreLocomotion()
{
    if (BoundMesh && PlayingSequence && CapturedLocomotionClass &&
        BoundMesh->GetAnimationMode() == EAnimationMode::AnimationSingleNode)
    {
        // Do not overwrite a newer driver installed by another presentation owner.
        UAnimSingleNodeInstance* SingleNode = BoundMesh->GetSingleNodeInstance();
        if (SingleNode && SingleNode->GetCurrentAsset() == PlayingSequence)
        {
            BoundMesh->SetAnimInstanceClass(CapturedLocomotionClass);
            BoundMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        }
    }
    PlayingSequence = nullptr;
}

void UOGDiagnosticAnimationPresentation::RefreshState()
{
    if (!PresentState(PresentedState, false))
    {
        RestoreLocomotion();
    }
}

void UOGDiagnosticAnimationPresentation::SetPresentedState(FName State)
{
    if (BoundMesh && ResolvedRoleMesh.Get() != BoundMesh->GetSkeletalMeshAsset()) ResolveBoneRoles();
    if (PresentedState == State)
    {
        // Actor/mode hiding may have relinquished the overlay while canonical
        // traversal stayed in the same state. Reassert only missing fallback
        // ownership; never restart authored clips every actor tick.
        if (ActionToken == 0 && ProceduralState.IsNone() && !HasCompatibleBinding(State) &&
            HasProceduralBinding(State) && OwnsMeshDriver() && GetOwner() && !GetOwner()->IsHidden() &&
            !BoundMesh->bHiddenInGame && BoundMesh->IsVisible()) RefreshState();
        return;
    }
    if (State == FName(TEXT("Death")))
    { ActionToken = 0; ActionRemainingSeconds = 0.0f; }
    PresentedState = State;
    if (ActionToken == 0) RefreshState();
}

int32 UOGDiagnosticAnimationPresentation::PresentTimedAction(
    FName Action, float PresentationSeconds)
{
    if (!FMath::IsFinite(PresentationSeconds) || PresentationSeconds <= 0.0f ||
        !PresentState(Action, true))
    {
        return 0;
    }
    NextToken = NextToken == MAX_int32 ? 1 : NextToken + 1;
    ActionToken = NextToken;
    ActionRemainingSeconds = FMath::Clamp(PresentationSeconds, 0.01f, 5.0f);
    return ActionToken;
}

void UOGDiagnosticAnimationPresentation::ClearTimedAction(int32 Token)
{
    if (Token == 0 || Token != ActionToken) return;
    ActionToken = 0;
    ActionRemainingSeconds = 0.0f;
    RefreshState();
}

void UOGDiagnosticAnimationPresentation::TickComponent(float DeltaTime,
    ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateProceduralPose(DeltaTime);
    if (ActionToken != 0)
    {
        ActionRemainingSeconds -= DeltaTime;
        if (ActionRemainingSeconds <= 0.0f) ClearTimedAction(ActionToken);
    }
}

void UOGDiagnosticAnimationPresentation::EndPlay(const EEndPlayReason::Type Reason)
{
    UnbindMesh();
    Super::EndPlay(Reason);
}
namespace
{
    FTransform DiagnosticComponentBone(const UPoseableMeshComponent* Mesh, int32 Index)
    {
        const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Mesh ? Mesh->GetSkinnedAsset() : nullptr);
        if (!SkeletalMesh || !Mesh->BoneSpaceTransforms.IsValidIndex(Index))
            return FTransform::Identity;
        const FReferenceSkeleton& Ref = SkeletalMesh->GetRefSkeleton();
        FTransform Result = Mesh->BoneSpaceTransforms[Index];
        for (int32 Parent = Ref.GetParentIndex(Index); Parent != INDEX_NONE; Parent = Ref.GetParentIndex(Parent))
        {
            if (!Mesh->BoneSpaceTransforms.IsValidIndex(Parent)) break;
            Result = Result * Mesh->BoneSpaceTransforms[Parent];
        }
        return Result;
    }
}

void UOGDiagnosticAnimationPresentation::ResetPresentation()
{
    ActionToken = 0;
    ActionRemainingSeconds = 0.0f;
    PresentedState = NAME_None;
    EndProceduralPose(true);
    RestoreLocomotion();
}

void UOGDiagnosticAnimationPresentation::UnbindMesh()
{
    ResetPresentation();
    if (BoundMesh) RemoveTickPrerequisiteComponent(BoundMesh);
    BoundMesh = nullptr;
    CapturedLocomotionClass = nullptr;
    PlayingSequence = nullptr;
    BoneRoles.Reset();
    ResolvedRoleMesh.Reset();
}

bool UOGDiagnosticAnimationPresentation::OwnsMeshDriver() const
{
    if (!BoundMesh || !CapturedLocomotionClass) return false;
    if (const UAnimInstance* Instance = BoundMesh->GetAnimInstance())
        if (Instance->GetCurrentActiveMontage()) return false;
    if (BoundMesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint)
        return BoundMesh->GetAnimClass() == CapturedLocomotionClass;
    UAnimSingleNodeInstance* Single = BoundMesh->GetSingleNodeInstance();
    return PlayingSequence && Single && Single->GetCurrentAsset() == PlayingSequence;
}

void UOGDiagnosticAnimationPresentation::ResolveBoneRoles()
{
    BoneRoles.Reset();
    ResolvedRoleMesh = BoundMesh ? BoundMesh->GetSkeletalMeshAsset() : nullptr;
    if (!BoundMesh || !BoundMesh->GetSkeletalMeshAsset()) return;
    const FReferenceSkeleton& Ref = BoundMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    auto FindRole = [&](FName Role, std::initializer_list<const TCHAR*> Aliases)
    {
        for (const TCHAR* Alias : Aliases)
            for (int32 Index = 0; Index < Ref.GetNum(); ++Index)
            {
                FString Name = Ref.GetBoneName(Index).ToString().ToLower();
                // Namespaces from imported rigs do not change anatomical roles.
                int32 Separator = INDEX_NONE;
                if (Name.FindLastChar(TEXT(':'), Separator)) Name = Name.Mid(Separator + 1);
                Name.ReplaceInline(TEXT("_"), TEXT("")); Name.ReplaceInline(TEXT(" "), TEXT(""));
                if (Name == FString(Alias).ToLower()) { BoneRoles.Add(Role, Index); return; }
            }
    };
    FindRole(TEXT("root"), {TEXT("root"), TEXT("hips"), TEXT("pelvis")});
    FindRole(TEXT("pelvis"), {TEXT("pelvis"), TEXT("hips"), TEXT("bippelvis")});
    FindRole(TEXT("spine"), {TEXT("spine01"), TEXT("spine"), TEXT("bipspine")});
    FindRole(TEXT("chest"), {TEXT("spine03"), TEXT("spine02"), TEXT("spine2"), TEXT("chest")});
    FindRole(TEXT("head"), {TEXT("head"), TEXT("biphead")});
    FindRole(TEXT("arm_l"), {TEXT("upperarml"), TEXT("leftarm"), TEXT("arml")});
    FindRole(TEXT("arm_r"), {TEXT("upperarmr"), TEXT("rightarm"), TEXT("armr")});
    FindRole(TEXT("forearm_l"), {TEXT("lowerarml"), TEXT("leftforearm"), TEXT("forearml")});
    FindRole(TEXT("forearm_r"), {TEXT("lowerarmr"), TEXT("rightforearm"), TEXT("forearmr")});
    FindRole(TEXT("hand_l"), {TEXT("handl"), TEXT("lefthand")});
    FindRole(TEXT("hand_r"), {TEXT("handr"), TEXT("righthand")});
    FindRole(TEXT("thigh_l"), {TEXT("thighl"), TEXT("leftupleg"), TEXT("uplegl")});
    FindRole(TEXT("thigh_r"), {TEXT("thighr"), TEXT("rightupleg"), TEXT("uplegr")});
    FindRole(TEXT("shin_l"), {TEXT("calfl"), TEXT("leftleg"), TEXT("lowerlegl")});
    FindRole(TEXT("shin_r"), {TEXT("calfr"), TEXT("rightleg"), TEXT("lowerlegr")});
    FindRole(TEXT("foot_l"), {TEXT("footl"), TEXT("leftfoot")});
    FindRole(TEXT("foot_r"), {TEXT("footr"), TEXT("rightfoot")});
}

bool UOGDiagnosticAnimationPresentation::HasProceduralBinding(FName Id) const
{
    if (!bEnableProceduralDiagnosticPoses || !BoundMesh || !BoundMesh->GetSkeletalMeshAsset() ||
        !BoneRoles.Contains(TEXT("pelvis")) || !BoneRoles.Contains(TEXT("spine")) ||
        !BoneRoles.Contains(TEXT("arm_l")) || !BoneRoles.Contains(TEXT("arm_r")) ||
        !BoneRoles.Contains(TEXT("thigh_l")) || !BoneRoles.Contains(TEXT("thigh_r"))) return false;
    const FString Name = Id.ToString().ToLower();
    return Name == TEXT("turn") || Name == TEXT("jump") || Name == TEXT("fall") ||
        Name == TEXT("land") || Name == TEXT("dodge") || Name == TEXT("hit") ||
        Name == TEXT("death") || Name.StartsWith(TEXT("attack")) || Name == TEXT("basicattack") ||
        Name.StartsWith(TEXT("skill")) || Name == TEXT("ultimate") || Name == TEXT("interact") ||
        Name == TEXT("climb") || Name == TEXT("swim") || Name == TEXT("dive") ||
        Name == TEXT("fly") || Name == TEXT("mount") || Name == TEXT("mounted") ||
        Name == TEXT("vehicle") || Name == TEXT("enemytelegraph") || Name == TEXT("enemyattack");
}

bool UOGDiagnosticAnimationPresentation::PresentState(FName State, bool bRestart)
{
    if (PlayBoundClip(State, bRestart)) return true;
    RestoreLocomotion();
    if (BeginProceduralPose(State, bRestart)) return true;
    EndProceduralPose();
    return false;
}

bool UOGDiagnosticAnimationPresentation::BeginProceduralPose(FName State, bool bRestart)
{
    if (!OwnsMeshDriver() || !HasProceduralBinding(State) || !GetOwner() ||
        GetOwner()->IsHidden() || BoundMesh->bHiddenInGame ||
        (!bOwnsSourceVisibility && !BoundMesh->IsVisible())) return false;
    if (!DiagnosticPoseMesh)
    {
        DiagnosticPoseMesh = NewObject<UPoseableMeshComponent>(GetOwner(), NAME_None, RF_Transient);
        GetOwner()->AddInstanceComponent(DiagnosticPoseMesh);
        DiagnosticPoseMesh->SetupAttachment(BoundMesh);
        DiagnosticPoseMesh->SetRelativeTransform(FTransform::Identity);
        DiagnosticPoseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        DiagnosticPoseMesh->SetGenerateOverlapEvents(false);
        DiagnosticPoseMesh->SetComponentTickEnabled(false);
        DiagnosticPoseMesh->ComponentTags.Add(TEXT("OG.DiagnosticPose"));
        DiagnosticPoseMesh->SetSkinnedAssetAndUpdate(BoundMesh->GetSkeletalMeshAsset());
        DiagnosticPoseMesh->RegisterComponent();
    }
    if (!bOwnsSourceVisibility)
    {
        bSavedSourceVisibility = BoundMesh->IsVisible();
        bOwnsSourceVisibility = true;
        // Keep the captured locomotion pose advancing while its neutral overlay
        // is visible. Restore this property only while we still own its value.
        SavedSourceTickPolicy = static_cast<uint8>(BoundMesh->VisibilityBasedAnimTickOption);
        BoundMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        bOwnsSourceTickPolicy = true;
        BoundMesh->SetVisibility(false, false);
    }
    if (bRestart || ProceduralState != State) ProceduralSeconds = 0.0f;
    ProceduralState = State;
    DiagnosticPoseMesh->SetVisibility(true, false);
    UpdateProceduralPose(0.0f);
    return true;
}

void UOGDiagnosticAnimationPresentation::EndProceduralPose(bool bDestroy)
{
    if (DiagnosticPoseMesh) DiagnosticPoseMesh->SetVisibility(false, false);
    if (BoundMesh && bOwnsSourceVisibility && !BoundMesh->IsVisible())
        BoundMesh->SetVisibility(bSavedSourceVisibility, false);
    if (BoundMesh && bOwnsSourceTickPolicy &&
        BoundMesh->VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones)
        BoundMesh->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(SavedSourceTickPolicy);
    bOwnsSourceVisibility = false;
    bOwnsSourceTickPolicy = false;
    ProceduralState = NAME_None;
    ProceduralSeconds = 0.0f;
    if (bDestroy && DiagnosticPoseMesh)
    {
        DiagnosticPoseMesh->SetSkinnedAssetAndUpdate(nullptr);
        DiagnosticPoseMesh->OverrideMaterials.Reset();
        DiagnosticPoseMesh->DestroyComponent();
        DiagnosticPoseMesh = nullptr;
    }
}

void UOGDiagnosticAnimationPresentation::RotateRole(FName Role, const FVector& Axis, float Radians)
{
    const int32* Index = BoneRoles.Find(Role);
    if (!DiagnosticPoseMesh || !Index || !DiagnosticPoseMesh->BoneSpaceTransforms.IsValidIndex(*Index) ||
        Axis.IsNearlyZero() || !FMath::IsFinite(Radians)) return;
    const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(DiagnosticPoseMesh->GetSkinnedAsset());
    if (!SkeletalMesh) return;
    const FReferenceSkeleton& Ref = SkeletalMesh->GetRefSkeleton();
    const int32 ParentIndex = Ref.GetParentIndex(*Index);
    const FQuat ParentRotation = ParentIndex == INDEX_NONE ? FQuat::Identity :
        DiagnosticComponentBone(DiagnosticPoseMesh, ParentIndex).GetRotation();
    // Axis is derived in component space from actual body/owner geometry. Never
    // assume a particular imported bone's local Euler axes.
    const FQuat LocalDelta = ParentRotation.Inverse() * FQuat(Axis.GetSafeNormal(), Radians) * ParentRotation;
    FTransform& Local = DiagnosticPoseMesh->BoneSpaceTransforms[*Index];
    Local.SetRotation((LocalDelta * Local.GetRotation()).GetNormalized());
}

void UOGDiagnosticAnimationPresentation::AimRole(FName Role, FName Child, const FVector& Direction, float Strength)
{
    const int32* Bone = BoneRoles.Find(Role); const int32* Tip = BoneRoles.Find(Child);
    if (!Bone || !Tip || !DiagnosticPoseMesh || Direction.IsNearlyZero()) return;
    const FVector From = DiagnosticComponentBone(DiagnosticPoseMesh, *Tip).GetLocation() -
        DiagnosticComponentBone(DiagnosticPoseMesh, *Bone).GetLocation();
    if (From.IsNearlyZero()) return;
    const FQuat Delta = FQuat::Slerp(FQuat::Identity,
        FQuat::FindBetweenNormals(From.GetSafeNormal(), Direction.GetSafeNormal()), FMath::Clamp(Strength, 0.f, 1.f));
    FVector Axis; double Angle;
    Delta.ToAxisAndAngle(Axis, Angle); RotateRole(Role, Axis, Angle);
}

void UOGDiagnosticAnimationPresentation::UpdateProceduralPose(float DeltaTime)
{
    if (ProceduralState.IsNone() || !DiagnosticPoseMesh) return;
    // Actor hiding (Ruler mode, privacy, staging) takes precedence. A different
    // animation owner or montage wins immediately; no driver is overwritten.
    if (!bEnableProceduralDiagnosticPoses || !BoundMesh || !GetOwner() ||
        GetOwner()->IsHidden() || BoundMesh->bHiddenInGame || !OwnsMeshDriver())
    { EndProceduralPose(); return; }
    if (DiagnosticPoseMesh->GetSkinnedAsset() != BoundMesh->GetSkeletalMeshAsset())
    {
        DiagnosticPoseMesh->OverrideMaterials.Reset();
        DiagnosticPoseMesh->SetSkinnedAssetAndUpdate(BoundMesh->GetSkeletalMeshAsset());
        ResolveBoneRoles();
        if (!HasProceduralBinding(ProceduralState)) { EndProceduralPose(); return; }
    }
    for (int32 Index = 0; Index < BoundMesh->GetNumMaterials(); ++Index)
        if (DiagnosticPoseMesh->GetMaterial(Index) != BoundMesh->GetMaterial(Index))
            DiagnosticPoseMesh->SetMaterial(Index, BoundMesh->GetMaterial(Index));
    const FString PoseName = ProceduralState.ToString().ToLower();
    const bool bTraversalPose = PoseName == TEXT("climb") || PoseName == TEXT("swim") ||
        PoseName == TEXT("dive") || PoseName == TEXT("fly") || PoseName == TEXT("mount") ||
        PoseName == TEXT("mounted") || PoseName == TEXT("vehicle") || PoseName == TEXT("death");
    // Traversal/death owns a complete diagnostic pose; inheriting the live
    // weapon locomotion pose leaves bent aiming arms in swimming/riding.
    if (bTraversalPose)
        DiagnosticPoseMesh->BoneSpaceTransforms = BoundMesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose();
    else
        DiagnosticPoseMesh->CopyPoseFromSkeletalComponent(BoundMesh);
    if (FMath::IsFinite(DeltaTime)) ProceduralSeconds += FMath::Max(0.f, DeltaTime);
    const FTransform Frame = BoundMesh->GetComponentTransform();
    const FVector Forward = Frame.InverseTransformVectorNoScale(GetOwner()->GetActorForwardVector()).GetSafeNormal();
    const FVector Right = Frame.InverseTransformVectorNoScale(GetOwner()->GetActorRightVector()).GetSafeNormal();
    FVector Up = Frame.InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
    // Reference torso geometry gives a rig-independent anatomical up direction.
    if (const int32* Pelvis = BoneRoles.Find(TEXT("pelvis")))
        if (const int32* Head = BoneRoles.Find(TEXT("head")))
        {
            const FReferenceSkeleton& Ref = BoundMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
            TArray<FTransform> Reference = Ref.GetRefBonePose();
            for (int32 Index = 1; Index < Reference.Num(); ++Index)
                if (Ref.GetParentIndex(Index) != INDEX_NONE)
                    Reference[Index] = Reference[Index] * Reference[Ref.GetParentIndex(Index)];
            const FVector AnatomicalUp = Reference[*Head].GetLocation() - Reference[*Pelvis].GetLocation();
            if (!AnatomicalUp.IsNearlyZero()) Up = AnatomicalUp.GetSafeNormal();
        }
    const float Cycle = FMath::Sin(ProceduralSeconds * 6.f);
    const float Pulse = FMath::Sin(FMath::Clamp(ProceduralSeconds / .32f, 0.f, 1.f) * PI);
    const FString State = ProceduralState.ToString().ToLower();
    auto Arms = [&](FVector Left, FVector RightDirection, float Blend = 1.f)
    {
        AimRole(TEXT("arm_l"), TEXT("forearm_l"), Left, Blend);
        AimRole(TEXT("arm_r"), TEXT("forearm_r"), RightDirection, Blend);
    };
    auto Crouch = [&](float Amount)
    {
        RotateRole(TEXT("thigh_l"), Right, -.45f * Amount);
        RotateRole(TEXT("thigh_r"), Right, -.45f * Amount);
        RotateRole(TEXT("shin_l"), Right, .85f * Amount);
        RotateRole(TEXT("shin_r"), Right, .85f * Amount);
        RotateRole(TEXT("spine"), Right, .15f * Amount);
    };
    if (State == TEXT("climb"))
    {
        Arms(Up + Forward + Right * (.15f * Cycle), Up + Forward - Right * (.15f * Cycle));
        RotateRole(TEXT("thigh_l"), Right, -.6f - .3f * Cycle);
        RotateRole(TEXT("thigh_r"), Right, -.6f + .3f * Cycle);
        RotateRole(TEXT("shin_l"), Right, .8f); RotateRole(TEXT("shin_r"), Right, .8f);
    }
    else if (State == TEXT("swim") || State == TEXT("dive") || State == TEXT("fly"))
    {
        // Positive pitch moves anatomical up/head toward actor forward.
        // Negative pitch made the neutral swimmer visibly travel feet-first.
        RotateRole(TEXT("pelvis"), Right, State == TEXT("fly") ? -.25f : .95f);
        Arms(Forward - Right * .5f + Up * (.2f * Cycle), Forward + Right * .5f - Up * (.2f * Cycle));
        AimRole(TEXT("forearm_l"), TEXT("hand_l"), Forward - Right * .25f, 1.0f);
        AimRole(TEXT("forearm_r"), TEXT("hand_r"), Forward + Right * .25f, 1.0f);
        RotateRole(TEXT("thigh_l"), Right, .2f * Cycle); RotateRole(TEXT("thigh_r"), Right, -.2f * Cycle);
    }
    else if (State == TEXT("mount") || State == TEXT("mounted") || State == TEXT("vehicle"))
    {
        Crouch(1.3f); Arms(Forward - Right * .2f, Forward + Right * .2f);
    }
    else if (State == TEXT("jump"))
    {
        Crouch(.55f); Arms(Up - Right * .3f, Up + Right * .3f, .8f);
    }
    else if (State == TEXT("fall"))
    {
        Arms(-Right + Up * .35f, Right + Up * .35f, .85f); Crouch(.2f);
    }
    else if (State == TEXT("land")) Crouch(1.f - FMath::Clamp(ProceduralSeconds / .22f, 0.f, 1.f));
    else if (State == TEXT("dodge"))
    {
        Crouch(1.f); RotateRole(TEXT("spine"), Forward, -.55f * Pulse);
        Arms(-Up - Right * .2f, -Up + Right * .2f);
    }
    else if (State == TEXT("death"))
    {
        RotateRole(TEXT("root"), Right, -1.3f * FMath::Clamp(ProceduralSeconds / .5f, 0.f, 1.f));
        Arms(-Up - Right * .15f, -Up + Right * .15f); Crouch(.3f);
    }
    else if (State == TEXT("hit"))
    {
        RotateRole(TEXT("spine"), Right, -.4f * Pulse); Arms(Forward + Up * .4f, Forward + Up * .4f, .6f);
    }
    else if (State == TEXT("turn")) RotateRole(TEXT("spine"), Up, .35f * Cycle);
    else if (State == TEXT("interact"))
    {
        AimRole(TEXT("arm_r"), TEXT("forearm_r"), Forward - Up * .1f, .9f);
        AimRole(TEXT("forearm_r"), TEXT("hand_r"), Forward, .9f);
    }
    else if (State.StartsWith(TEXT("skill")) || State == TEXT("ultimate"))
    {
        Arms(Up - Right * .25f + Forward * .3f, Up + Right * .25f + Forward * .3f, .85f);
        RotateRole(TEXT("spine"), Up, .2f * Pulse);
    }
    else if (State == TEXT("enemytelegraph"))
    {
        Arms(Forward - Right * .4f + Up * .3f, Up + Right * .3f);
        RotateRole(TEXT("spine"), Up, -.35f);
    }
    else // Basic/chain/enemy attack: neutral reach and torso rotation.
    {
        const float Side = State.EndsWith(TEXT("2")) ? -1.f : 1.f;
        const FName Arm = Side < 0 ? FName(TEXT("arm_l")) : FName(TEXT("arm_r"));
        const FName Forearm = Side < 0 ? FName(TEXT("forearm_l")) : FName(TEXT("forearm_r"));
        const FName Hand = Side < 0 ? FName(TEXT("hand_l")) : FName(TEXT("hand_r"));
        AimRole(Arm, Forearm, Forward + Right * (.4f * Side * (1.f - Pulse)), .9f);
        AimRole(Forearm, Hand, Forward, .9f);
        RotateRole(TEXT("spine"), Up, .45f * Side * Pulse);
    }
    DiagnosticPoseMesh->MarkRefreshTransformDirty();
    DiagnosticPoseMesh->RefreshBoneTransforms();
}
